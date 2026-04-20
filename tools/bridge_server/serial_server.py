"""SerialOverTcpServer: bridges a local serial port to a single TCP client."""

import asyncio
import logging
import serial

logger = logging.getLogger(__name__)


class SerialOverTcpServer:
    def __init__(self, serial_port: str, baudrate: int, tcp_port: int):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.tcp_port = tcp_port
        self._serial: serial.Serial | None = None
        self._server: asyncio.AbstractServer | None = None
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None

    async def start(self) -> None:
        self._serial = serial.Serial(
            self.serial_port,
            self.baudrate,
            timeout=0,
        )
        self._serial.reset_input_buffer()
        logger.info(
            "Serial port %s opened at %d baud", self.serial_port, self.baudrate
        )

        self._server = await asyncio.start_server(
            self._handle_client, "0.0.0.0", self.tcp_port
        )
        logger.info("Serial TCP server listening on port %d", self.tcp_port)

    async def stop(self) -> None:
        if self._writer is not None:
            self._writer.close()
            await self._writer.wait_closed()
            self._writer = None
            self._reader = None

        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None

        if self._serial is not None:
            self._serial.close()
            self._serial = None

    async def _handle_client(
        self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter
    ) -> None:
        peer = writer.get_extra_info("peername")
        logger.info("Serial client connected: %s", peer)

        if self._writer is not None:
            logger.warning("Rejecting second client %s — only one allowed", peer)
            writer.close()
            await writer.wait_closed()
            return

        self._reader = reader
        self._writer = writer

        serial_to_tcp_task = asyncio.create_task(self._serial_to_tcp())
        tcp_to_serial_task = asyncio.create_task(self._tcp_to_serial())

        try:
            done, _ = await asyncio.wait(
                {serial_to_tcp_task, tcp_to_serial_task},
                return_when=asyncio.FIRST_COMPLETED,
            )

            for task in done:
                exc = task.exception()
                if exc is not None:
                    raise exc
        except asyncio.CancelledError:
            pass
        except Exception:
            logger.exception("Serial bridge error")
        finally:
            for task in (serial_to_tcp_task, tcp_to_serial_task):
                if not task.done():
                    task.cancel()

            await asyncio.gather(
                serial_to_tcp_task,
                tcp_to_serial_task,
                return_exceptions=True,
            )

            logger.info("Serial client disconnected: %s", peer)
            self._writer.close()
            await self._writer.wait_closed()
            self._writer = None
            self._reader = None

    async def _serial_to_tcp(self) -> None:
        loop = asyncio.get_event_loop()
        assert self._serial is not None
        assert self._writer is not None

        while True:
            data = await loop.run_in_executor(None, self._serial_read_blocking)
            if data:
                self._writer.write(data)
                await self._writer.drain()

    def _serial_read_blocking(self) -> bytes:
        assert self._serial is not None
        self._serial.timeout = 0.05
        data = self._serial.read(1)
        if data:
            remaining = self._serial.in_waiting
            if remaining > 0:
                data += self._serial.read(remaining)
        return data

    async def _tcp_to_serial(self) -> None:
        assert self._serial is not None
        assert self._reader is not None

        while True:
            data = await self._reader.read(4096)
            if not data:
                break
            self._serial.write(data)
