#!/usr/bin/env python3
"""
Bridge Server — exposes local serial and CAN hardware over TCP.

Run on the host machine with direct hardware access. C++ clients inside
a Docker container connect via TCP to access the hardware transparently.

Usage examples:
    # Serial only
    python bridge_server.py --serial-port COM3

    # CAN only (PCAN on Windows)
    python bridge_server.py --can-interface pcan --can-channel PCAN_USBBUS1

    # CAN with CANable (slcan) on Linux
    python bridge_server.py --can-interface slcan --can-channel /dev/ttyACM0

    # CAN with CANable (slcan) on Windows
    python bridge_server.py --can-interface slcan --can-channel COM3

    # CAN with CANable (gs_usb / candleLight) on Windows/Linux
    python bridge_server.py --can-interface gs_usb --can-channel 0

    # CAN with CANable (Candle API, no WinUSB driver swap needed, Windows)
    python bridge_server.py --can-interface candle --can-channel 0

    # List all CAN interfaces/channels detected on this machine
    python bridge_server.py --list-can
    python bridge_server.py --list-can --json

    # Both serial and CAN (SocketCAN on Linux)
    python bridge_server.py \\
        --serial-port /dev/ttyACM0 --serial-baudrate 921600 \\
        --can-interface socketcan --can-channel can0 --can-bitrate 500000
"""

import argparse
import asyncio
import logging
import signal
import sys

from serial_server import SerialOverTcpServer
from can_server import CanBusOverTcpServer
from server_errors import BridgeServerError

logger = logging.getLogger(__name__)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bridge serial and CAN hardware to TCP for remote access."
    )

    serial_group = parser.add_argument_group("Serial")
    serial_group.add_argument(
        "--serial-port",
        help="Serial port device (e.g. COM3, /dev/ttyACM0). Omit to disable.",
    )
    serial_group.add_argument(
        "--serial-baudrate", type=int, default=921600, help="Serial baudrate (default: 921600)"
    )
    serial_group.add_argument(
        "--serial-tcp-port", type=int, default=5000, help="TCP port for serial bridge (default: 5000)"
    )

    can_group = parser.add_argument_group("CAN bus")
    can_group.add_argument(
        "--can-interface",
        help="python-can interface type (e.g. socketcan, pcan, slcan, gs_usb, candle). Omit to disable.",
    )
    can_group.add_argument(
        "--can-channel",
        default=None,
        help="CAN channel (e.g. can0, PCAN_USBBUS1, /dev/ttyACM0, COM3). Required when --can-interface is set.",
    )
    can_group.add_argument(
        "--can-bitrate", type=int, default=125000, help="CAN bitrate (default: 125000)"
    )
    can_group.add_argument(
        "--can-tty-baudrate",
        type=int,
        default=115200,
        help="Serial baudrate for slcan adapters like CANable (default: 115200)",
    )
    can_group.add_argument(
        "--can-tcp-port", type=int, default=5001, help="TCP port for CAN bridge (default: 5001)"
    )
    can_group.add_argument(
        "--list-can",
        action="store_true",
        help="List all CAN interfaces and channels available on this machine, then exit.",
    )
    can_group.add_argument(
        "--json",
        action="store_true",
        dest="output_json",
        help="With --list-can: output as JSON array instead of a table.",
    )

    parser.add_argument(
        "--bind",
        default="127.0.0.1",
        help="Address to listen on. Defaults to loopback; the bridge has no authentication or "
        "transport security, so use 0.0.0.0 only on a trusted, isolated network (default: 127.0.0.1)",
    )

    parser.add_argument(
        "--log-level",
        default="INFO",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging level (default: INFO)",
    )

    return parser.parse_args()


async def main() -> None:
    args = parse_args()
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    )

    if getattr(args, "list_can", False):
        from list_can_interfaces import gather_all, format_table
        import json as _json
        configs = gather_all()
        if args.output_json:
            print(_json.dumps(configs, indent=2))
        else:
            print(format_table(configs))
        return

    if args.serial_port is None and args.can_interface is None:
        logger.error("At least one of --serial-port or --can-interface must be specified.")
        sys.exit(1)

    if args.can_interface is not None and args.can_channel is None:
        if args.can_interface in ("gs_usb", "candle"):
            args.can_channel = "0"
        else:
            logger.error("--can-channel is required when --can-interface is specified.")
            sys.exit(1)

    servers: list[SerialOverTcpServer | CanBusOverTcpServer] = []

    try:
        if args.serial_port is not None:
            serial_srv = SerialOverTcpServer(
                serial_port=args.serial_port,
                baudrate=args.serial_baudrate,
                tcp_port=args.serial_tcp_port,
                bind_address=args.bind,
            )
            await serial_srv.start()
            servers.append(serial_srv)

        if args.can_interface is not None:
            can_srv = CanBusOverTcpServer(
                interface=args.can_interface,
                channel=args.can_channel,
                bitrate=args.can_bitrate,
                tcp_port=args.can_tcp_port,
                tty_baudrate=args.can_tty_baudrate if args.can_interface == "slcan" else None,
                bind_address=args.bind,
            )
            await can_srv.start()
            servers.append(can_srv)
    except BridgeServerError as exc:
        logger.error("Bridge server startup failed: %s", exc)
        for srv in reversed(servers):
            await srv.stop()
        sys.exit(1)
    except Exception:
        logger.exception("Unexpected bridge server startup failure")
        for srv in reversed(servers):
            await srv.stop()
        sys.exit(1)

    logger.info("Bridge server running. Press Ctrl+C to stop.")

    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()

    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop_event.set)
        except NotImplementedError:
            signal.signal(sig, lambda *_: stop_event.set())

    try:
        await stop_event.wait()
    finally:
        logger.info("Shutting down...")
        for srv in servers:
            await srv.stop()


if __name__ == "__main__":
    asyncio.run(main())
