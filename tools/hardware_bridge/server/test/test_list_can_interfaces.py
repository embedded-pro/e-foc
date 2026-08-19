"""Tests for list_can_interfaces — all external dependencies are stubbed per test class."""

import json
import pathlib
import sys
import types
import unittest
from unittest import mock


SERVER_DIR = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SERVER_DIR))

# ---------------------------------------------------------------------------
# Minimal module-level stubs are required only to allow imports of modules
# that transitively import can / candle_driver / serial at load time.
# list_can_interfaces.py imports those lazily (inside functions), so these
# stubs are just a safety net and are NOT relied on by the test assertions.
# ---------------------------------------------------------------------------
if "can" not in sys.modules:
    _stub_can_bus = types.ModuleType("can.bus")
    _stub_can = types.ModuleType("can")
    _stub_can.BusABC = object
    _stub_can.detect_available_configs = mock.Mock(return_value=[])
    _stub_can.bus = _stub_can_bus
    sys.modules["can"] = _stub_can
    sys.modules["can.bus"] = _stub_can_bus

if "candle_driver" not in sys.modules:
    _stub_candle = types.ModuleType("candle_driver")
    _stub_candle.CANDLE_ID_EXTENDED = 0x80000000
    _stub_candle.list_devices = mock.Mock(return_value=[])
    sys.modules["candle_driver"] = _stub_candle

if "serial" not in sys.modules:
    _stub_serial_lp = types.ModuleType("serial.tools.list_ports")
    _stub_serial_lp.comports = mock.Mock(return_value=[])
    _stub_serial_tools = types.ModuleType("serial.tools")
    _stub_serial_tools.list_ports = _stub_serial_lp
    _stub_serial = types.ModuleType("serial")
    _stub_serial.Serial = mock.Mock()
    _stub_serial.tools = _stub_serial_tools
    sys.modules["serial"] = _stub_serial
    sys.modules["serial.tools"] = _stub_serial_tools
    sys.modules["serial.tools.list_ports"] = _stub_serial_lp

import list_can_interfaces


# ---------------------------------------------------------------------------
# Helpers to build isolated stubs for each test class
# ---------------------------------------------------------------------------

def _make_can_stub(return_value=None):
    """Return a fresh (can, can.bus) stub pair with detect_available_configs mocked."""
    stub_bus = types.ModuleType("can.bus")
    stub_bus_state = mock.Mock(name="BusState")
    stub_bus_state.ACTIVE = mock.sentinel.BUS_STATE_ACTIVE
    stub_bus.BusState = stub_bus_state

    stub_can = types.ModuleType("can")
    stub_can.BusABC = object
    stub_can.Bus = mock.Mock(name="Bus")
    stub_can.detect_available_configs = mock.Mock(return_value=return_value or [])
    stub_can.bus = stub_bus
    return stub_can, stub_bus


def _make_candle_stub(devices=None):
    stub = types.ModuleType("candle_driver")
    stub.CANDLE_ID_EXTENDED = 0x80000000
    stub.list_devices = mock.Mock(return_value=devices or [])
    return stub


def _make_serial_stub(ports=None):
    stub_lp = types.ModuleType("serial.tools.list_ports")
    stub_lp.comports = mock.Mock(return_value=ports or [])
    stub_tools = types.ModuleType("serial.tools")
    stub_tools.list_ports = stub_lp
    stub_serial = types.ModuleType("serial")
    stub_serial.tools = stub_tools
    return stub_serial, stub_tools, stub_lp


# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------

class TestDetectPythonCanConfigs(unittest.TestCase):
    def setUp(self):
        self._can, self._can_bus = _make_can_stub()
        self._patcher = mock.patch.dict(
            sys.modules, {"can": self._can, "can.bus": self._can_bus}
        )
        self._patcher.start()

    def tearDown(self):
        self._patcher.stop()

    def test_returns_normalized_entries_for_found_configs(self):
        self._can.detect_available_configs.return_value = [
            {"interface": "socketcan", "channel": "can0"},
            {"interface": "socketcan", "channel": "can1"},
        ]

        result = list_can_interfaces.detect_python_can_configs(interfaces=["socketcan"])

        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["interface"], "socketcan")
        self.assertEqual(result[0]["channel"], "can0")
        self.assertEqual(result[0]["source"], "python-can")
        self.assertEqual(result[1]["channel"], "can1")

    def test_propagates_extra_backend_metadata(self):
        self._can.detect_available_configs.return_value = [
            {"interface": "pcan", "channel": "PCAN_USBBUS1", "supports_fd": True},
        ]

        result = list_can_interfaces.detect_python_can_configs(interfaces=["pcan"])

        self.assertEqual(len(result), 1)
        self.assertTrue(result[0]["supports_fd"])

    def test_returns_empty_list_when_no_configs_found(self):
        self._can.detect_available_configs.return_value = []

        result = list_can_interfaces.detect_python_can_configs(interfaces=["socketcan"])

        self.assertEqual(result, [])

    def test_skips_backend_on_exception_and_continues(self):
        def side_effect(interfaces):
            if "pcan" in interfaces:
                raise RuntimeError("PCAN driver not found")
            return [{"interface": "socketcan", "channel": "can0"}]

        self._can.detect_available_configs.side_effect = side_effect

        result = list_can_interfaces.detect_python_can_configs(
            interfaces=["pcan", "socketcan"]
        )

        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["interface"], "socketcan")

    def test_returns_empty_list_when_can_not_installed(self):
        with mock.patch.dict(sys.modules, {"can": None}):
            result = list_can_interfaces.detect_python_can_configs(interfaces=["socketcan"])

        self.assertEqual(result, [])


class TestDetectCandleDevices(unittest.TestCase):
    def setUp(self):
        self._candle = _make_candle_stub()
        self._patcher = mock.patch.dict(sys.modules, {"candle_driver": self._candle})
        self._patcher.start()

    def tearDown(self):
        self._patcher.stop()

    def test_returns_one_entry_per_device(self):
        device_a = mock.Mock()
        device_a.name = mock.Mock(return_value="CANable-v2 #0")
        device_b = mock.Mock()
        device_b.name = mock.Mock(return_value="CANable-v2 #1")
        self._candle.list_devices.return_value = [device_a, device_b]

        result = list_can_interfaces.detect_candle_devices()

        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["interface"], "candle")
        self.assertEqual(result[0]["channel"], "0")
        self.assertEqual(result[0]["source"], "candle_driver")
        self.assertEqual(result[1]["channel"], "1")

    def test_returns_empty_list_when_no_devices(self):
        self._candle.list_devices.return_value = []

        result = list_can_interfaces.detect_candle_devices()

        self.assertEqual(result, [])

    def test_handles_list_devices_exception_gracefully(self):
        self._candle.list_devices.side_effect = OSError("USB error")

        result = list_can_interfaces.detect_candle_devices()

        self.assertEqual(result, [])

    def test_returns_empty_when_candle_driver_not_installed(self):
        with mock.patch.dict(sys.modules, {"candle_driver": None}):
            result = list_can_interfaces.detect_candle_devices()

        self.assertEqual(result, [])


class TestDetectSlcanSerialPorts(unittest.TestCase):
    def setUp(self):
        self._serial, self._serial_tools, self._list_ports = _make_serial_stub()
        self._patcher = mock.patch.dict(
            sys.modules,
            {
                "serial": self._serial,
                "serial.tools": self._serial_tools,
                "serial.tools.list_ports": self._list_ports,
            },
        )
        self._patcher.start()

    def tearDown(self):
        self._patcher.stop()

    def test_returns_one_entry_per_serial_port(self):
        port_a = mock.Mock()
        port_a.device = "/dev/ttyACM0"
        port_a.description = "CANable USB to CAN adapter"
        port_b = mock.Mock()
        port_b.device = "/dev/ttyACM1"
        port_b.description = "USB Serial"
        self._list_ports.comports.return_value = [port_a, port_b]

        result = list_can_interfaces.detect_slcan_serial_ports()

        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["interface"], "slcan")
        self.assertEqual(result[0]["channel"], "/dev/ttyACM0")
        self.assertEqual(result[0]["details"], "CANable USB to CAN adapter")
        self.assertEqual(result[0]["source"], "serial-ports")
        self.assertEqual(result[1]["channel"], "/dev/ttyACM1")

    def test_returns_empty_list_when_no_ports(self):
        self._list_ports.comports.return_value = []

        result = list_can_interfaces.detect_slcan_serial_ports()

        self.assertEqual(result, [])

    def test_returns_empty_when_pyserial_not_installed(self):
        with mock.patch.dict(sys.modules, {"serial.tools.list_ports": None}):
            result = list_can_interfaces.detect_slcan_serial_ports()

        self.assertEqual(result, [])


class TestGatherAll(unittest.TestCase):
    def test_aggregates_all_three_sources(self):
        with (
            mock.patch.object(
                list_can_interfaces,
                "detect_python_can_configs",
                return_value=[{"interface": "socketcan", "channel": "can0", "source": "python-can"}],
            ),
            mock.patch.object(
                list_can_interfaces,
                "detect_candle_devices",
                return_value=[{"interface": "candle", "channel": "0", "source": "candle_driver"}],
            ),
            mock.patch.object(
                list_can_interfaces,
                "detect_slcan_serial_ports",
                return_value=[{"interface": "slcan", "channel": "/dev/ttyACM0", "source": "serial-ports"}],
            ),
        ):
            result = list_can_interfaces.gather_all()

        self.assertEqual(len(result), 3)
        interfaces = {r["interface"] for r in result}
        self.assertEqual(interfaces, {"socketcan", "candle", "slcan"})

    def test_deduplicates_on_interface_and_channel(self):
        duplicate_entry = {"interface": "socketcan", "channel": "can0", "source": "python-can"}
        with (
            mock.patch.object(
                list_can_interfaces,
                "detect_python_can_configs",
                return_value=[duplicate_entry, duplicate_entry],
            ),
            mock.patch.object(list_can_interfaces, "detect_candle_devices", return_value=[]),
            mock.patch.object(list_can_interfaces, "detect_slcan_serial_ports", return_value=[]),
        ):
            result = list_can_interfaces.gather_all()

        self.assertEqual(len(result), 1)

    def test_passes_interfaces_filter_to_python_can(self):
        with (
            mock.patch.object(
                list_can_interfaces,
                "detect_python_can_configs",
                return_value=[],
            ) as mock_detect,
            mock.patch.object(list_can_interfaces, "detect_candle_devices", return_value=[]),
            mock.patch.object(list_can_interfaces, "detect_slcan_serial_ports", return_value=[]),
        ):
            list_can_interfaces.gather_all(interfaces=["socketcan"])

        mock_detect.assert_called_once_with(["socketcan"])


class TestFormatTable(unittest.TestCase):
    def test_returns_no_detected_message_for_empty_list(self):
        result = list_can_interfaces.format_table([])

        self.assertIn("No CAN interfaces detected", result)

    def test_table_contains_header_and_separator(self):
        configs = [{"interface": "socketcan", "channel": "can0", "source": "python-can"}]

        result = list_can_interfaces.format_table(configs)

        lines = result.splitlines()
        self.assertIn("Interface", lines[0])
        self.assertIn("Channel", lines[0])
        self.assertIn("Source", lines[0])
        self.assertRegex(lines[1], r"^-+")

    def test_table_contains_config_data(self):
        configs = [
            {"interface": "socketcan", "channel": "can0", "source": "python-can"},
            {"interface": "candle", "channel": "0", "details": "CANable", "source": "candle_driver"},
        ]

        result = list_can_interfaces.format_table(configs)

        self.assertIn("socketcan", result)
        self.assertIn("can0", result)
        self.assertIn("candle", result)
        self.assertIn("CANable", result)

    def test_columns_are_aligned(self):
        configs = [
            {"interface": "socketcan", "channel": "can0", "source": "python-can"},
            {"interface": "gs_usb", "channel": "0", "source": "python-can"},
        ]

        result = list_can_interfaces.format_table(configs)

        lines = result.splitlines()
        header_channel_pos = lines[0].index("Channel")
        data_row_pos = lines[2].index(configs[0]["channel"])
        self.assertEqual(header_channel_pos, data_row_pos)


class TestBridgeServerListCan(unittest.IsolatedAsyncioTestCase):
    """Verify --list-can causes bridge_server to print & exit without starting servers."""

    async def test_list_can_prints_table_and_returns(self):
        import bridge_server as bs

        fake_configs = [{"interface": "socketcan", "channel": "can0", "source": "python-can"}]
        fake_table = "Interface  Channel  Details  Source\n-----\nsocketcan  can0             python-can"

        with (
            mock.patch("list_can_interfaces.gather_all", return_value=fake_configs),
            mock.patch("list_can_interfaces.format_table", return_value=fake_table),
            mock.patch("sys.argv", ["bridge_server.py", "--list-can"]),
            mock.patch("builtins.print") as mock_print,
        ):
            await bs.main()

        mock_print.assert_called_once_with(fake_table)

    async def test_list_can_json_outputs_json(self):
        import bridge_server as bs

        fake_configs = [{"interface": "socketcan", "channel": "can0", "source": "python-can"}]

        with (
            mock.patch("list_can_interfaces.gather_all", return_value=fake_configs),
            mock.patch("sys.argv", ["bridge_server.py", "--list-can", "--json"]),
            mock.patch("builtins.print") as mock_print,
        ):
            await bs.main()

        printed = mock_print.call_args[0][0]
        parsed = json.loads(printed)
        self.assertEqual(parsed, fake_configs)

    async def test_list_can_does_not_require_serial_or_can_interface(self):
        """--list-can must bypass the 'specify at least one interface' guard."""
        import bridge_server as bs

        with (
            mock.patch("list_can_interfaces.gather_all", return_value=[]),
            mock.patch(
                "list_can_interfaces.format_table",
                return_value="No CAN interfaces detected.",
            ),
            mock.patch("sys.argv", ["bridge_server.py", "--list-can"]),
            mock.patch("builtins.print"),
            mock.patch.object(bs, "CanBusOverTcpServer") as mock_can_srv,
            mock.patch.object(bs, "SerialOverTcpServer") as mock_serial_srv,
        ):
            await bs.main()

        mock_can_srv.assert_not_called()
        mock_serial_srv.assert_not_called()


if __name__ == "__main__":
    unittest.main()
