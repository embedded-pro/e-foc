"""
Data structures and exception types for cycle analysis.
"""

from __future__ import annotations

from dataclasses import dataclass, field


# ──────────────────────────────────────────────────────────────────────────────
# Core data structures
# ──────────────────────────────────────────────────────────────────────────────

@dataclass
class Instruction:
    address: int
    raw_bytes: str
    mnemonic: str
    operands: str
    size: int
    min_cycles: int = 1
    max_cycles: int = 1


@dataclass
class Function:
    name: str
    demangled: str
    start_addr: int
    end_addr: int = 0
    instructions: list = field(default_factory=list)
    total_min_cycles: int = 0
    total_max_cycles: int = 0
    instruction_count: int = 0
    code_size: int = 0
    calls: list = field(default_factory=list)
    fpu_ops: int = 0
    load_store_ops: int = 0
    branch_ops: int = 0
    alu_ops: int = 0
    mul_div_ops: int = 0


@dataclass
class PathStage:
    """One stage in the configured critical path."""
    label: str
    functions: list[str] = field(default_factory=list)   # resolved demangled names
    min_cycles: int = 0
    max_cycles: int = 0
    instruction_count: int = 0
    code_size: int = 0
    fpu_ops: int = 0
    is_overhead: bool = False    # True for ISR entry/exit (no disasm)


# ──────────────────────────────────────────────────────────────────────────────
# Exception types
# ──────────────────────────────────────────────────────────────────────────────

class StageMatchError(RuntimeError):
    """Raised when a path stage matches no functions in the binary."""


class ConfigError(RuntimeError):
    """Raised when the configuration file is invalid."""
