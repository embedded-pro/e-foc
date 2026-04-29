"""SerialOverTcpServer: bridges a local serial port to a single TCP client."""

import asyncio
import errno
import logging
import serial

from server_errors import HardwareUnavailableError, PortUnavailableError

logger = logging.getLogger(__name__)


class SerialOverTcpServer:
    def __init__(
        self,
        serial_port: str,
        baudrate: int,
        tcp_port: int,
        serial_factory=serial.Serial,
        server_factory=asyncio.start_server,
    ):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.tcp_port = tcp_port
        self._serial_factory = serial_factory
        self._server_factory = server_factory
        self._serial: serial.Serial | None = None
        self._server: asyncio.AbstractServer | None = None
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None

    async def start(self) -> None:
        try:
            self._serial = self._serial_factory(
                self.serial_port,
                self.baudrate,
                timeout=0.05,
            )
            self._serial.reset_input_buffer()
        except Exception as exc:
            raise HardwareUnavailableError(
                f"Cannot open serial port {self.serial_port}: {exc}"
            ) from exc

        logger.info(
            "Serial port %s opened at %d baud", self.serial_port, self.baudrate
        )

        try:
            self._server = await self._server_factory(
                self._handle_client, "0.0.0.0", self.tcp_port
            )
        except OSError as exc:
            await self._close_serial()
            if exc.errno in (errno.EADDRINUSE, errno.EACCES):
                raise PortUnavailableError(
                    f"Cannot listen for serial bridge on TCP port {self.tcp_port}: {exc.strerror}"
                ) from exc
            raise
        except Exception:
            await self._close_serial()
            raise

        logger.info("Serial TCP server listening on port %d", self.tcp_port)

    async def stop(self) -> None:
        await self._close_writer(self._writer)

        if self._server is not None:
            self._server.close()
            await self._server.wait_closed()
            self._server = None

        await self._close_serial()

    async def _close_serial(self) -> None:
        if self._serial is not None:
            try:
                self._serial.close()
            except Exception:
                logger.warning("Error while closing serial port", exc_info=True)
            self._serial = None

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
        logger.info("Serial client connected: %s", peer)

        if self._writer is not None:
            logger.warning("Rejecting second client %s — only one allowed", peer)
            await self._close_writer(writer)
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
                task.result()
        except asyncio.CancelledError:
            pass
        except (ConnectionError, OSError) as exc:
            logger.warning("Serial bridge session ended: %s", exc)
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
            await self._close_writer(writer)

    async def _serial_to_tcp(self) -> None:
        loop = asyncio.get_running_loop()
        assert self._serial is not None
        assert self._writer is not None

        while True:
            data = await loop.run_in_executor(None, self._serial_read_blocking)
            if data:
                self._writer.write(data)
                await self._writer.drain()

    def _serial_read_blocking(self) -> bytes:
        assert self._serial is not None
        data = self._serial.read(1)
        if data:
            remaining = self._serial.in_waiting
            if remaining > 0:
                data += self._serial.read(remaining)
        return data

    async def _tcp_to_serial(self) -> None:
        loop = asyncio.get_running_loop()
        assert self._serial is not None
        assert self._reader is not None

        while True:
            data = await self._reader.read(4096)
            if not data:
                break
            await loop.run_in_executor(None, self._serial.write, data)
