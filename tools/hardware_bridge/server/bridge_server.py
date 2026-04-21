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
        help="python-can interface type (e.g. socketcan, pcan, slcan). Omit to disable.",
    )
    can_group.add_argument(
        "--can-channel",
        default="can0",
        help="CAN channel (e.g. can0, PCAN_USBBUS1, /dev/ttyACM0, COM3). Default: can0",
    )
    can_group.add_argument(
        "--can-bitrate", type=int, default=500000, help="CAN bitrate (default: 500000)"
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

    if args.serial_port is None and args.can_interface is None:
        logger.error("At least one of --serial-port or --can-interface must be specified.")
        sys.exit(1)

    servers: list[SerialOverTcpServer | CanBusOverTcpServer] = []

    if args.serial_port is not None:
        serial_srv = SerialOverTcpServer(
            serial_port=args.serial_port,
            baudrate=args.serial_baudrate,
            tcp_port=args.serial_tcp_port,
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
        )
        await can_srv.start()
        servers.append(can_srv)

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
