"""CanBusOverTcpServer: bridges a CAN bus interface to a single TCP client.

CAN frames are sent over TCP as SocketCAN-compatible struct can_frame (16 bytes):
    - 4 bytes: CAN ID (uint32, little-endian, bits 0-28 = ID, bit 29 = RTR,
               bit 30 = error, bit 31 = extended frame flag)
    - 1 byte:  DLC (data length code, 0-8)
    - 3 bytes: padding (zeroed)
    - 8 bytes: data (padded with zeros if DLC < 8)

Supported adapters:
    - SocketCAN (Linux native): interface=socketcan, channel=can0
    - PCAN (Windows/Linux):     interface=pcan, channel=PCAN_USBBUS1
    - CANable (slcan):          interface=slcan, channel=/dev/ttyACM0 or COM3
    - Any python-can backend:   pass the appropriate interface/channel
"""

import asyncio
import errno
import logging
import struct
from typing import Any

import can

from server_errors import HardwareUnavailableError, PortUnavailableError

logger = logging.getLogger(__name__)

CAN_FRAME_FORMAT = "<IBxxx8s"
CAN_FRAME_SIZE = 16
CAN_EFF_FLAG = 0x80000000
CAN_ERR_FLAG = 0x20000000


def _build_bus_kwargs(
    interface: str,
    channel: str,
    bitrate: int,
    tty_baudrate: int | None = None,
) -> dict[str, Any]:
    kwargs: dict[str, Any] = {
        "interface": interface,
        "channel": channel,
        "bitrate": bitrate,
    }

    if interface == "slcan":
        kwargs["ttyBaudrate"] = tty_baudrate or 115200

    return kwargs


class CanBusOverTcpServer:
    def __init__(
        self,
        interface: str,
        channel: str,
        bitrate: int,
        tcp_port: int,
        tty_baudrate: int | None = None,
        bus_factory=can.Bus,
        server_factory=asyncio.start_server,
    ):
        self.interface = interface
        self.channel = channel
        self.bitrate = bitrate
        self.tcp_port = tcp_port
        self.tty_baudrate = tty_baudrate
        self._bus_factory = bus_factory
        self._server_factory = server_factory
        self._bus: can.BusABC | None = None
        self._server: asyncio.AbstractServer | None = None
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None

    async def start(self) -> None:
        bus_kwargs = _build_bus_kwargs(
            self.interface, self.channel, self.bitrate, self.tty_baudrate
        )
        try:
            self._bus = self._bus_factory(**bus_kwargs)
        except Exception as exc:
            raise HardwareUnavailableError(
                f"Cannot open CAN bus {self.interface}:{self.channel}: {exc}"
            ) from exc

        logger.info(
            "CAN bus opened: interface=%s channel=%s bitrate=%d%s",
            self.interface,
            self.channel,
            self.bitrate,
            f" ttyBaudrate={self.tty_baudrate}" if self.interface == "slcan" else "",
        )

        try:
            self._server = await self._server_factory(
                self._handle_client, "0.0.0.0", self.tcp_port
            )
        except OSError as exc:
            await self._close_bus()
            if exc.errno in (errno.EADDRINUSE, errno.EACCES):
                raise PortUnavailableError(
                    f"Cannot listen for CAN bridge on TCP port {self.tcp_port}: {exc.strerror}"
                ) from exc
            raise
        except Exception:
            await self._close_bus()
            raise

        logger.info("CAN TCP server listening on port %d", self.tcp_port)

    async def stop(self) -> None:
        await self._close_writer(self._writer)

        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None

        await self._close_bus()

    async def _close_bus(self) -> None:
        if self._bus is not None:
            try:
                self._bus.shutdown()
            except Exception:
                logger.warning("Error while shutting down CAN bus", exc_info=True)
            self._bus = None

    async def _close_writer(self, writer: asyncio.StreamWriter | None) -> None:
        if writer is None:
            return

        if self._writer is writer:
            self._writer = None
            self._reader = None

        writer.close()
        try:
            await writer.wait_closed()
        except (ConnectionError, OSError, RuntimeError):
            logger.debug("TCP writer already closed", exc_info=True)

    async def _handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        peer = writer.get_extra_info("peername")
        logger.info("CAN client connected: %s", peer)

        if self._writer is not None:
            logger.warning("Rejecting second client %s — only one allowed", peer)
            await self._close_writer(writer)
            return

        self._reader = reader
        self._writer = writer

        can_to_tcp_task = asyncio.create_task(self._can_to_tcp())
        tcp_to_can_task = asyncio.create_task(self._tcp_to_can())

        try:
            done, pending = await asyncio.wait(
                {can_to_tcp_task, tcp_to_can_task},
                return_when=asyncio.FIRST_COMPLETED,
            )

            for task in done:
                task.result()

            for task in pending:
                task.cancel()

            if pending:
                await asyncio.gather(*pending, return_exceptions=True)
        except asyncio.CancelledError:
            pass
        except (ConnectionError, OSError) as exc:
            logger.warning("CAN bridge session ended: %s", exc)
        except Exception:
            logger.exception("CAN bridge error")
        finally:
            for task in (can_to_tcp_task, tcp_to_can_task):
                if not task.done():
                    task.cancel()

            await asyncio.gather(
                can_to_tcp_task,
                tcp_to_can_task,
                return_exceptions=True,
            )

            logger.info("CAN client disconnected: %s", peer)
            await self._close_writer(writer)

    async def _can_to_tcp(self) -> None:
        loop = asyncio.get_running_loop()
        assert self._bus is not None
        assert self._writer is not None

        while True:
            msg = await loop.run_in_executor(None, self._can_recv_blocking)
            if msg is not None:
                frame = self._encode_frame(msg)
                self._writer.write(frame)
                await self._writer.drain()

    def _can_recv_blocking(self) -> can.Message | None:
        assert self._bus is not None
        return self._bus.recv(timeout=0.05)

    async def _tcp_to_can(self) -> None:
        assert self._bus is not None
        assert self._reader is not None
        buf = b""

        while True:
            data = await self._reader.read(4096)
            if not data:
                break
            buf += data

            while len(buf) >= CAN_FRAME_SIZE:
                raw_frame = buf[:CAN_FRAME_SIZE]
                buf = buf[CAN_FRAME_SIZE:]
                msg = self._decode_frame(raw_frame)
                if msg is not None:
                    loop = asyncio.get_running_loop()
                    await loop.run_in_executor(None, self._bus.send, msg)

        if buf:
            logger.warning("Dropping partial CAN frame of %d byte(s)", len(buf))

    @staticmethod
    def _encode_frame(msg: can.Message) -> bytes:
        can_id = msg.arbitration_id
        if msg.is_extended_id:
            can_id |= CAN_EFF_FLAG
        data_padded = msg.data.ljust(8, b"\x00")
        return struct.pack(CAN_FRAME_FORMAT, can_id, msg.dlc, bytes(data_padded))

    @staticmethod
    def _decode_frame(raw: bytes) -> can.Message | None:
        if len(raw) != CAN_FRAME_SIZE:
            return None
        can_id, dlc, data = struct.unpack(CAN_FRAME_FORMAT, raw)
        if can_id & CAN_ERR_FLAG:
            return None
        is_extended = bool(can_id & CAN_EFF_FLAG)
        arb_id = can_id & 0x1FFFFFFF
        dlc = min(dlc, 8)
        return can.Message(
            arbitration_id=arb_id,
            is_extended_id=is_extended,
            dlc=dlc,
            data=data[:dlc],
        )
