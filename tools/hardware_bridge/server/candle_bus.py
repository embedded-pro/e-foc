"""CandleBus: python-can–compatible adapter for the Candle API (Windows).

Wraps candle_driver so CandleBus can be used wherever can.BusABC is expected.
Supports CANable and other candleLight-firmware adapters using the Candle API
Windows driver (as installed by CANgaroo). No WinUSB / libusb required.

Usage:
    from candle_bus import CandleBus
    bus = CandleBus(channel=0, bitrate=500000)
    msg = bus.recv(timeout=0.1)
    bus.send(can.Message(arbitration_id=0x123, dlc=1, data=b'\\x01'))
    bus.shutdown()
"""

from __future__ import annotations

import can
import candle_driver

# Extended-frame and RTR flag bits in the Candle API can_id word.
_CANDLE_ID_EXTENDED = candle_driver.CANDLE_ID_EXTENDED
_CANDLE_ID_RTR = 0x40000000


class CandleBus:
    """Minimal can.BusABC–compatible wrapper around candle_driver.

    Parameters
    ----------
    channel:
        Zero-based index of the Candle USB device to open (default 0).
        The first (and usually only) CAN channel of the device is always used.
    bitrate:
        CAN bus bitrate in bits/s (default 500 000).
    **_kwargs:
        Extra keyword arguments are silently ignored so the class accepts the
        same kwargs dict produced by can_server._build_bus_kwargs.
    """

    def __init__(
        self,
        channel: int = 0,
        bitrate: int = 500000,
        **_kwargs,
    ) -> None:
        devices = candle_driver.list_devices()
        if not devices:
            raise RuntimeError("No Candle USB devices found")
        if channel >= len(devices):
            raise RuntimeError(
                f"Candle device index {channel} out of range"
                f" ({len(devices)} device(s) found)"
            )
        self._device = devices[channel]
        self._device.open()
        self._ch = self._device.channel(0)
        self._ch.set_bitrate(bitrate)
        self._ch.start()

    # ------------------------------------------------------------------
    # Public interface expected by can_server.CanBusOverTcpServer
    # ------------------------------------------------------------------

    def recv(self, timeout: float = 0.05) -> can.Message | None:
        """Block up to *timeout* seconds and return the next CAN frame.

        Returns None on timeout instead of raising, matching can.BusABC
        semantics.
        """
        timeout_ms = max(1, int(timeout * 1000))
        try:
            _frame_type, can_id, can_data, extended, _ts = self._ch.read(timeout_ms)
        except TimeoutError:
            return None
        dlc = len(can_data)
        return can.Message(
            arbitration_id=can_id,
            is_extended_id=bool(extended),
            dlc=dlc,
            data=can_data,
        )

    def send(self, msg: can.Message) -> None:
        """Transmit a CAN frame."""
        raw_id = msg.arbitration_id
        if msg.is_extended_id:
            raw_id |= _CANDLE_ID_EXTENDED
        if msg.is_remote_frame:
            raw_id |= _CANDLE_ID_RTR
            data = b""
        else:
            data = bytes(msg.data[: msg.dlc])
        self._ch.write(raw_id, data)

    def shutdown(self) -> None:
        """Stop the channel and close the device."""
        try:
            self._ch.stop()
        except Exception:
            pass
        try:
            self._device.close()
        except Exception:
            pass
