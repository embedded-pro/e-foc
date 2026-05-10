import argparse
import asyncio
import errno
import pathlib
import struct
import sys
import types
import unittest
from unittest import mock


SERVER_DIR = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SERVER_DIR))


class FakeCanMessage:
    def __init__(self, arbitration_id, is_extended_id=False, dlc=0, data=b"", is_remote_frame=False):
        self.arbitration_id = arbitration_id
        self.is_extended_id = is_extended_id
        self.dlc = dlc
        self.data = bytes(data)
        self.is_remote_frame = is_remote_frame


fake_serial_module = types.ModuleType("serial")
fake_serial_module.Serial = mock.Mock(name="Serial")
sys.modules["serial"] = fake_serial_module

fake_can_bus_module = types.ModuleType("can.bus")
fake_can_bus_state = mock.Mock(name="BusState")
fake_can_bus_state.ACTIVE = mock.sentinel.BUS_STATE_ACTIVE
fake_can_bus_module.BusState = fake_can_bus_state

fake_can_module = types.ModuleType("can")
fake_can_module.Message = FakeCanMessage
fake_can_module.BusABC = object
fake_can_module.Bus = mock.Mock(name="Bus")
fake_can_module.bus = fake_can_bus_module
sys.modules["can"] = fake_can_module
sys.modules["can.bus"] = fake_can_bus_module

# Stub out candle_driver so CandleBus can be imported without the real library.
fake_candle_driver_module = types.ModuleType("candle_driver")
fake_candle_driver_module.CANDLE_ID_EXTENDED = 0x80000000
fake_candle_driver_module.list_devices = mock.Mock(return_value=[])
sys.modules["candle_driver"] = fake_candle_driver_module

import bridge_server
import can_server
import candle_bus
import serial_server
from server_errors import BridgeServerError, PortUnavailableError


class FakeAsyncServer:
    def __init__(self):
        self.closed = False
        self.wait_closed_called = False

    def close(self):
        self.closed = True

    async def wait_closed(self):
        self.wait_closed_called = True


class FakeReader:
    def __init__(self, chunks):
        self.chunks = list(chunks)

    async def read(self, _size):
        await asyncio.sleep(0)
        if self.chunks:
            return self.chunks.pop(0)
        return b""


class FakeWriter:
    def __init__(self, drain_exception=None, wait_closed_exception=None):
        self.writes = []
        self.closed = False
        self.wait_closed_called = False
        self.drain_exception = drain_exception
        self.wait_closed_exception = wait_closed_exception

    def get_extra_info(self, name):
        if name == "peername":
            return ("127.0.0.1", 12345)
        return None

    def write(self, data):
        self.writes.append(data)

    async def drain(self):
        if self.drain_exception is not None:
            raise self.drain_exception

    def close(self):
        self.closed = True

    async def wait_closed(self):
        self.wait_closed_called = True
        if self.wait_closed_exception is not None:
            raise self.wait_closed_exception


class FakeSerialDevice:
    def __init__(self, read_chunks=None, write_exception=None):
        self.read_chunks = list(read_chunks or [])
        self.write_exception = write_exception
        self.reset_called = False
        self.closed = False
        self.writes = []

    @property
    def in_waiting(self):
        if self.read_chunks:
            return len(self.read_chunks[0])
        return 0

    def reset_input_buffer(self):
        self.reset_called = True

    def read(self, _size):
        if self.read_chunks:
            return self.read_chunks.pop(0)
        return b""

    def write(self, data):
        if self.write_exception is not None:
            raise self.write_exception
        self.writes.append(data)

    def close(self):
        self.closed = True


class FakeBus:
    def __init__(self, recv_messages=None, send_exception=None):
        self.recv_messages = list(recv_messages or [])
        self.send_exception = send_exception
        self.sent = []
        self.shutdown_called = False

    def recv(self, timeout):
        if self.recv_messages:
            message = self.recv_messages.pop(0)
            if isinstance(message, BaseException):
                raise message
            return message
        return None

    def send(self, message):
        if self.send_exception is not None:
            raise self.send_exception
        self.sent.append(message)

    def shutdown(self):
        self.shutdown_called = True


def can_frame(can_id, dlc=0, data=b""):
    return struct.pack(
        can_server.CAN_FRAME_FORMAT,
        can_id,
        dlc,
        bytes(data).ljust(8, b"\x00"),
    )


class TestSerialServerBadWeather(unittest.IsolatedAsyncioTestCase):
    async def test_start_reports_port_in_use_and_closes_serial(self):
        device = FakeSerialDevice()

        async def port_in_use_server_factory(_handler, _host, _port):
            raise OSError(errno.EADDRINUSE, "Address already in use")

        server = serial_server.SerialOverTcpServer(
            "COM3",
            921600,
            5000,
            serial_factory=lambda *_args, **_kwargs: device,
            server_factory=port_in_use_server_factory,
        )

        with self.assertRaises(PortUnavailableError):
            await server.start()

        self.assertTrue(device.reset_called)
        self.assertTrue(device.closed)
        self.assertIsNone(server._serial)

    async def test_client_disconnect_closes_session_and_keeps_server_available(self):
        device = FakeSerialDevice()
        server = serial_server.SerialOverTcpServer(
            "COM3", 921600, 5000, serial_factory=lambda *_args, **_kwargs: device
        )
        server._serial = device
        reader = FakeReader([b"abc", b""])
        writer = FakeWriter()

        await server._handle_client(reader, writer)

        self.assertEqual(device.writes, [b"abc"])
        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIsNone(server._reader)

    async def test_serial_write_failure_closes_only_the_session(self):
        device = FakeSerialDevice(write_exception=OSError("device removed"))
        server = serial_server.SerialOverTcpServer(
            "COM3", 921600, 5000, serial_factory=lambda *_args, **_kwargs: device
        )
        server._serial = device
        reader = FakeReader([b"abc"])
        writer = FakeWriter()

        with self.assertLogs(serial_server.logger, level="WARNING") as logs:
            await server._handle_client(reader, writer)

        self.assertIn("Serial bridge session ended", "\n".join(logs.output))
        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIs(server._serial, device)

    async def test_serial_read_failure_closes_only_the_session(self):
        class ReadFailingSerialDevice(FakeSerialDevice):
            def read(self, _size):
                raise OSError("device unplugged")

        device = ReadFailingSerialDevice()
        server = serial_server.SerialOverTcpServer(
            "COM3", 921600, 5000, serial_factory=lambda *_args, **_kwargs: device
        )
        server._serial = device
        reader = FakeReader([])
        writer = FakeWriter()

        with self.assertLogs(serial_server.logger, level="WARNING") as logs:
            await server._handle_client(reader, writer)

        self.assertIn("Serial bridge session ended", "\n".join(logs.output))
        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIs(server._serial, device)

    async def test_second_serial_client_is_rejected_without_disturbing_active_client(self):
        device = FakeSerialDevice()
        active_writer = FakeWriter()
        second_writer = FakeWriter()
        server = serial_server.SerialOverTcpServer(
            "COM3", 921600, 5000, serial_factory=lambda *_args, **_kwargs: device
        )
        server._serial = device
        server._writer = active_writer

        await server._handle_client(FakeReader([b""]), second_writer)

        self.assertTrue(second_writer.closed)
        self.assertIs(server._writer, active_writer)
        self.assertFalse(active_writer.closed)

    async def test_writer_wait_closed_failure_is_treated_as_already_closed(self):
        server = serial_server.SerialOverTcpServer("COM3", 921600, 5000)
        writer = FakeWriter(wait_closed_exception=ConnectionResetError("gone"))
        server._writer = writer

        await server._close_writer(writer)

        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)


class TestCanServerBadWeather(unittest.IsolatedAsyncioTestCase):
    async def test_start_reports_port_in_use_and_shuts_down_bus(self):
        bus = FakeBus()

        async def port_in_use_server_factory(_handler, _host, _port):
            raise OSError(errno.EADDRINUSE, "Address already in use")

        server = can_server.CanBusOverTcpServer(
            "socketcan",
            "can0",
            500000,
            5001,
            bus_factory=lambda **_kwargs: bus,
            server_factory=port_in_use_server_factory,
        )

        with self.assertRaises(PortUnavailableError):
            await server.start()

        self.assertTrue(bus.shutdown_called)
        self.assertIsNone(server._bus)

    async def test_client_disconnect_closes_can_session(self):
        bus = FakeBus()
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        writer = FakeWriter()

        await server._handle_client(FakeReader([b""]), writer)

        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIsNone(server._reader)

    async def test_can_send_failure_closes_only_the_session(self):
        bus = FakeBus(send_exception=OSError("adapter removed"))
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        writer = FakeWriter()

        with self.assertLogs(can_server.logger, level="WARNING") as logs:
            await server._handle_client(FakeReader([can_frame(0x123, 1, b"\xAA")]), writer)

        self.assertIn("CAN bridge session ended", "\n".join(logs.output))
        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIs(server._bus, bus)

    async def test_can_receive_failure_closes_only_the_session(self):
        bus = FakeBus(recv_messages=[OSError("adapter removed")])
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        writer = FakeWriter()

        with self.assertLogs(can_server.logger, level="WARNING") as logs:
            await server._handle_client(FakeReader([]), writer)

        self.assertIn("CAN bridge session ended", "\n".join(logs.output))
        self.assertTrue(writer.closed)
        self.assertIsNone(server._writer)
        self.assertIs(server._bus, bus)

    async def test_partial_can_frame_is_dropped_when_client_disconnects(self):
        bus = FakeBus()
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        server._reader = FakeReader([b"partial", b""])

        with self.assertLogs(can_server.logger, level="WARNING") as logs:
            await server._tcp_to_can()

        self.assertFalse(bus.sent)
        self.assertIn("Dropping partial CAN frame", "\n".join(logs.output))

    async def test_error_can_frame_is_ignored(self):
        bus = FakeBus()
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        server._reader = FakeReader([can_frame(can_server.CAN_ERR_FLAG, 1, b"\xAA"), b""])

        await server._tcp_to_can()

        self.assertFalse(bus.sent)

    async def test_second_can_client_is_rejected_without_disturbing_active_client(self):
        bus = FakeBus()
        active_writer = FakeWriter()
        second_writer = FakeWriter()
        server = can_server.CanBusOverTcpServer(
            "socketcan", "can0", 500000, 5001, bus_factory=lambda **_kwargs: bus
        )
        server._bus = bus
        server._writer = active_writer

        await server._handle_client(FakeReader([b""]), second_writer)

        self.assertTrue(second_writer.closed)
        self.assertIs(server._writer, active_writer)
        self.assertFalse(active_writer.closed)

    def test_build_bus_kwargs_gs_usb_converts_channel_to_int(self):
        kwargs = can_server._build_bus_kwargs("gs_usb", "0", 500000)

        self.assertEqual(kwargs["interface"], "gs_usb")
        self.assertEqual(kwargs["bitrate"], 500000)
        self.assertIsInstance(kwargs["channel"], int)
        self.assertEqual(kwargs["channel"], 0)
        self.assertNotIn("ttyBaudrate", kwargs)

    def test_build_bus_kwargs_gs_usb_non_zero_index(self):
        kwargs = can_server._build_bus_kwargs("gs_usb", "1", 1000000)

        self.assertIsInstance(kwargs["channel"], int)
        self.assertEqual(kwargs["channel"], 1)

    def test_build_bus_kwargs_candle_converts_channel_to_int(self):
        kwargs = can_server._build_bus_kwargs("candle", "0", 500000)

        self.assertEqual(kwargs["interface"], "candle")
        self.assertEqual(kwargs["bitrate"], 500000)
        self.assertIsInstance(kwargs["channel"], int)
        self.assertEqual(kwargs["channel"], 0)
        self.assertNotIn("ttyBaudrate", kwargs)

    def test_build_bus_kwargs_candle_non_zero_index(self):
        kwargs = can_server._build_bus_kwargs("candle", "1", 1000000)

        self.assertIsInstance(kwargs["channel"], int)
        self.assertEqual(kwargs["channel"], 1)


class TestCandleBus(unittest.TestCase):
    def _make_fake_device(self, read_result=None, read_exception=None):
        fake_ch = mock.Mock()
        if read_exception is not None:
            fake_ch.read.side_effect = read_exception
        else:
            fake_ch.read.return_value = read_result
        fake_device = mock.Mock()
        fake_device.channel.return_value = fake_ch
        return fake_device, fake_ch

    def _patch_devices(self, devices):
        return mock.patch.object(
            fake_candle_driver_module, "list_devices", return_value=devices
        )

    def test_no_devices_raises_runtime_error(self):
        with self._patch_devices([]):
            with self.assertRaises(RuntimeError, msg="No Candle USB devices found"):
                candle_bus.CandleBus(channel=0, bitrate=500000)

    def test_channel_out_of_range_raises_runtime_error(self):
        device, _ = self._make_fake_device(read_exception=TimeoutError)
        with self._patch_devices([device]):
            with self.assertRaises(RuntimeError):
                candle_bus.CandleBus(channel=1, bitrate=500000)

    def test_recv_timeout_returns_none(self):
        device, ch = self._make_fake_device(read_exception=TimeoutError)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        ch.read.side_effect = TimeoutError
        result = bus.recv(timeout=0.05)

        self.assertIsNone(result)

    def test_recv_returns_can_message(self):
        frame = (0, 0x123, b"\xAB\xCD", False, 0)
        device, ch = self._make_fake_device(read_result=frame)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        ch.read.return_value = frame
        msg = bus.recv(timeout=0.1)

        self.assertIsNotNone(msg)
        self.assertEqual(msg.arbitration_id, 0x123)
        self.assertFalse(msg.is_extended_id)
        self.assertEqual(msg.dlc, 2)
        self.assertEqual(msg.data, b"\xAB\xCD")

    def test_recv_extended_frame(self):
        frame = (0, 0x1FFFF, b"\x01", True, 0)
        device, ch = self._make_fake_device(read_result=frame)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        ch.read.return_value = frame
        msg = bus.recv(timeout=0.1)

        self.assertTrue(msg.is_extended_id)
        self.assertEqual(msg.arbitration_id, 0x1FFFF)

    def test_send_standard_frame(self):
        device, ch = self._make_fake_device(read_exception=TimeoutError)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        msg = FakeCanMessage(0x100, is_extended_id=False, dlc=2, data=b"\x01\x02")
        bus.send(msg)

        ch.write.assert_called_once_with(0x100, b"\x01\x02")

    def test_send_extended_frame_sets_extended_flag(self):
        device, ch = self._make_fake_device(read_exception=TimeoutError)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        msg = FakeCanMessage(0x1FFFF, is_extended_id=True, dlc=1, data=b"\xAA")
        bus.send(msg)

        expected_id = 0x1FFFF | 0x80000000
        ch.write.assert_called_once_with(expected_id, b"\xAA")

    def test_shutdown_stops_channel_and_closes_device(self):
        device, ch = self._make_fake_device(read_exception=TimeoutError)
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        bus.shutdown()

        ch.stop.assert_called_once()
        device.close.assert_called_once()

    def test_shutdown_tolerates_errors(self):
        device, ch = self._make_fake_device(read_exception=TimeoutError)
        ch.stop.side_effect = OSError("gone")
        device.close.side_effect = OSError("gone")
        with self._patch_devices([device]):
            bus = candle_bus.CandleBus(channel=0, bitrate=500000)

        # Should not raise
        bus.shutdown()


class TestBridgeServerStartup(unittest.IsolatedAsyncioTestCase):
    async def test_startup_failure_stops_already_started_servers(self):
        stopped = []

        class StartedSerialServer:
            def __init__(self, **_kwargs):
                pass

            async def start(self):
                pass

            async def stop(self):
                stopped.append("serial")

        class FailingCanServer:
            def __init__(self, **_kwargs):
                pass

            async def start(self):
                raise BridgeServerError("can failed")

        args = argparse.Namespace(
            serial_port="COM3",
            serial_baudrate=921600,
            serial_tcp_port=5000,
            can_interface="socketcan",
            can_channel="can0",
            can_bitrate=500000,
            can_tcp_port=5001,
            can_tty_baudrate=115200,
            log_level="ERROR",
        )

        with mock.patch.object(bridge_server, "parse_args", return_value=args), \
            mock.patch.object(bridge_server, "SerialOverTcpServer", StartedSerialServer), \
            mock.patch.object(bridge_server, "CanBusOverTcpServer", FailingCanServer), \
            self.assertLogs(bridge_server.logger, level="ERROR") as logs, \
            self.assertRaises(SystemExit) as exit_context:
            await bridge_server.main()

        self.assertEqual(exit_context.exception.code, 1)
        self.assertEqual(stopped, ["serial"])
        self.assertIn("Bridge server startup failed", "\n".join(logs.output))

    async def test_unexpected_startup_failure_stops_already_started_servers(self):
        stopped = []

        class StartedSerialServer:
            def __init__(self, **_kwargs):
                pass

            async def start(self):
                pass

            async def stop(self):
                stopped.append("serial")

        class FailingCanServer:
            def __init__(self, **_kwargs):
                pass

            async def start(self):
                raise RuntimeError("unexpected can failure")

        args = argparse.Namespace(
            serial_port="COM3",
            serial_baudrate=921600,
            serial_tcp_port=5000,
            can_interface="socketcan",
            can_channel="can0",
            can_bitrate=500000,
            can_tcp_port=5001,
            can_tty_baudrate=115200,
            log_level="ERROR",
        )

        with mock.patch.object(bridge_server, "parse_args", return_value=args), \
            mock.patch.object(bridge_server, "SerialOverTcpServer", StartedSerialServer), \
            mock.patch.object(bridge_server, "CanBusOverTcpServer", FailingCanServer), \
            self.assertLogs(bridge_server.logger, level="ERROR") as logs, \
            self.assertRaises(SystemExit) as exit_context:
            await bridge_server.main()

        self.assertEqual(exit_context.exception.code, 1)
        self.assertEqual(stopped, ["serial"])
        self.assertIn("Unexpected bridge server startup failure", "\n".join(logs.output))

    async def test_gs_usb_defaults_channel_to_zero_when_omitted(self):
        captured = {}

        class CapturingCanServer:
            def __init__(self, **kwargs):
                captured.update(kwargs)

            async def start(self):
                raise BridgeServerError("stop early")

            async def stop(self):
                pass

        args = argparse.Namespace(
            serial_port=None,
            can_interface="gs_usb",
            can_channel=None,
            can_bitrate=500000,
            can_tcp_port=5001,
            can_tty_baudrate=115200,
            log_level="ERROR",
        )

        with mock.patch.object(bridge_server, "parse_args", return_value=args), \
            mock.patch.object(bridge_server, "CanBusOverTcpServer", CapturingCanServer), \
            self.assertLogs(bridge_server.logger, level="ERROR"), \
            self.assertRaises(SystemExit):
            await bridge_server.main()

        self.assertEqual(captured["channel"], "0")
        self.assertIsNone(captured["tty_baudrate"])

    async def test_candle_defaults_channel_to_zero_when_omitted(self):
        captured = {}

        class CapturingCanServer:
            def __init__(self, **kwargs):
                captured.update(kwargs)

            async def start(self):
                raise BridgeServerError("stop early")

            async def stop(self):
                pass

        args = argparse.Namespace(
            serial_port=None,
            can_interface="candle",
            can_channel=None,
            can_bitrate=500000,
            can_tcp_port=5001,
            can_tty_baudrate=115200,
            log_level="ERROR",
        )

        with mock.patch.object(bridge_server, "parse_args", return_value=args), \
            mock.patch.object(bridge_server, "CanBusOverTcpServer", CapturingCanServer), \
            self.assertLogs(bridge_server.logger, level="ERROR"), \
            self.assertRaises(SystemExit):
            await bridge_server.main()

        self.assertEqual(captured["channel"], "0")
        self.assertIsNone(captured["tty_baudrate"])


if __name__ == "__main__":
    unittest.main()
