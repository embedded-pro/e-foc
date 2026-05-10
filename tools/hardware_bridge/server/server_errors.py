"""Shared error types for the hardware bridge server."""


class BridgeServerError(RuntimeError):
    """Base class for expected bridge server failures."""


class HardwareUnavailableError(BridgeServerError):
    """Raised when a serial port or CAN adapter cannot be opened."""


class PortUnavailableError(BridgeServerError):
    """Raised when a TCP listen port cannot be opened."""
