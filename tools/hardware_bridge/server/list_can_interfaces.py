#!/usr/bin/env python3
"""
list_can_interfaces — discover CAN adapters and channels available on this machine.

Covers three detection sources:
  1. python-can's built-in detector  (socketcan, pcan, gs_usb, slcan, …)
  2. Candle API / candle_driver       (candleLight firmware, e.g. CANable 2.0 on Windows)
  3. Serial ports as slcan candidates (any USB-serial device could be a CANable in slcan mode)

Usage examples:
    python list_can_interfaces.py
    python list_can_interfaces.py --json
    python list_can_interfaces.py --interface socketcan
"""

from __future__ import annotations

import argparse
import json
import logging

logger = logging.getLogger(__name__)

# python-can interface backends that support detect_available_configs().
# Restrict the probe list so backends that hang or raise on import-only machines
# are not attempted unless explicitly requested.
_PROBED_INTERFACES = [
    "socketcan",
    "pcan",
    "gs_usb",
    "slcan",
    "kvaser",
    "ixxat",
    "nican",
    "vector",
    "virtual",
    "udp_multicast",
    "neousys",
    "etas",
    "cantact",
    "seeedstudio",
    "robotell",
    "usb2can",
    "iscan",
    "nixnet",
    "systec",
]


def detect_python_can_configs(interfaces: list[str] | None = None) -> list[dict]:
    """Return configs detected by python-can's built-in enumerator.

    Parameters
    ----------
    interfaces:
        If given, only these backend names are probed.  If None, the default
        set :data:`_PROBED_INTERFACES` is used.

    Returns
    -------
    list of dicts with keys ``interface``, ``channel``, ``source``, and any
    extra keys returned by the backend.
    """
    try:
        import can
    except ImportError:
        logger.warning("python-can is not installed; skipping python-can detection.")
        return []

    probe_list = interfaces if interfaces is not None else _PROBED_INTERFACES
    # Deduplicate while preserving order.
    probe_list = list(dict.fromkeys(probe_list))

    results: list[dict] = []
    for iface in probe_list:
        try:
            configs = can.detect_available_configs(interfaces=[iface])
        except Exception as exc:
            logger.debug("detect_available_configs(%s) raised: %s", iface, exc)
            continue
        for cfg in configs:
            entry = {
                "interface": cfg.get("interface", iface),
                "channel": str(cfg.get("channel", "")),
                "source": "python-can",
            }
            # Carry along any extra metadata the backend reports.
            for key, value in cfg.items():
                if key not in ("interface", "channel"):
                    entry[key] = value
            results.append(entry)

    return results


def detect_candle_devices() -> list[dict]:
    """Return one entry per Candle USB device found via candle_driver.

    The ``channel`` field is the zero-based device index that maps directly to
    ``--can-channel`` when using ``--can-interface candle``.

    Returns ``[]`` when the library is unavailable (e.g. on Linux without it).
    """
    try:
        import candle_driver
    except ImportError:
        logger.debug("candle_driver not installed; skipping Candle device detection.")
        return []

    try:
        devices = candle_driver.list_devices()
    except Exception as exc:
        logger.warning("candle_driver.list_devices() raised: %s", exc)
        return []

    results: list[dict] = []
    for idx, device in enumerate(devices):
        name: str = ""
        try:
            name = str(device.name()) if callable(getattr(device, "name", None)) else str(device)
        except Exception:
            name = f"device-{idx}"
        results.append(
            {
                "interface": "candle",
                "channel": str(idx),
                "details": name,
                "source": "candle_driver",
            }
        )
    return results


def detect_slcan_serial_ports() -> list[dict]:
    """Return serial ports that could be slcan adapters (e.g. CANable in slcan mode).

    These are *candidate* channels — any USB-serial device is listed.  The user
    must verify which one is their CAN adapter.

    Returns ``[]`` when pyserial is unavailable.
    """
    try:
        from serial.tools.list_ports import comports
    except ImportError:
        logger.debug("pyserial not installed; skipping serial port detection.")
        return []

    results: list[dict] = []
    for port in comports():
        results.append(
            {
                "interface": "slcan",
                "channel": port.device,
                "details": port.description or "",
                "source": "serial-ports",
            }
        )
    return results


def gather_all(interfaces: list[str] | None = None) -> list[dict]:
    """Aggregate all detection sources, deduplicating on (interface, channel).

    Parameters
    ----------
    interfaces:
        Optional list of python-can backend names to restrict probing.

    Returns
    -------
    Deduplicated list of config dicts.
    """
    seen: set[tuple[str, str]] = set()
    results: list[dict] = []

    for entry in (
        detect_python_can_configs(interfaces)
        + detect_candle_devices()
        + detect_slcan_serial_ports()
    ):
        key = (entry.get("interface", ""), entry.get("channel", ""))
        if key in seen:
            continue
        seen.add(key)
        results.append(entry)

    return results


def format_table(configs: list[dict]) -> str:
    """Render *configs* as a human-readable aligned text table.

    Returns a plain string with a header row, separator, and one data row per
    config.  Returns a single "none detected" line when the list is empty.
    """
    if not configs:
        return "No CAN interfaces detected."

    headers = ("Interface", "Channel", "Details", "Source")
    rows: list[tuple[str, str, str, str]] = []
    for cfg in configs:
        rows.append(
            (
                cfg.get("interface", ""),
                cfg.get("channel", ""),
                cfg.get("details", ""),
                cfg.get("source", ""),
            )
        )

    col_widths = [
        max(len(h), max(len(r[i]) for r in rows))
        for i, h in enumerate(headers)
    ]

    def _row(cells: tuple[str, ...]) -> str:
        return "  ".join(cell.ljust(col_widths[i]) for i, cell in enumerate(cells)).rstrip()

    sep = "  ".join("-" * w for w in col_widths)
    lines = [_row(headers), sep] + [_row(r) for r in rows]
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="List CAN interfaces and channels available on this machine."
    )
    parser.add_argument(
        "--interface",
        metavar="IFACE",
        help="Probe only this python-can backend (e.g. socketcan, pcan, gs_usb).",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        dest="output_json",
        help="Output as JSON array instead of a table.",
    )
    parser.add_argument(
        "--log-level",
        default="WARNING",
        choices=["DEBUG", "INFO", "WARNING", "ERROR"],
        help="Logging level (default: WARNING)",
    )
    args = parser.parse_args()

    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format="%(levelname)s: %(message)s",
    )

    interfaces = [args.interface] if args.interface else None
    configs = gather_all(interfaces)

    if args.output_json:
        print(json.dumps(configs, indent=2))
    else:
        print(format_table(configs))


if __name__ == "__main__":
    main()
