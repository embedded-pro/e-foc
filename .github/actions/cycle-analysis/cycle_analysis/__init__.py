"""
Public API re-exports for the cycle_analysis package.
"""

from cycle_analysis.analysis import analyze_stages, walk_call_graph
from cycle_analysis.models import (
    ConfigError,
    Function,
    Instruction,
    PathStage,
    StageMatchError,
)
from cycle_analysis.parser import parse_disassembly
from cycle_analysis.reports import (
    generate_annotated_disasm,
    generate_cpu_budget_table,
    generate_json_metrics,
    generate_pr_comment,
    generate_report,
)
from cycle_analysis.timing_model import EXCEPTION_OVERHEAD, TIMING
from cycle_analysis.tooling import log, run_objdump, run_size

__all__ = [
    "analyze_stages",
    "walk_call_graph",
    "ConfigError",
    "Function",
    "Instruction",
    "PathStage",
    "StageMatchError",
    "parse_disassembly",
    "generate_annotated_disasm",
    "generate_cpu_budget_table",
    "generate_json_metrics",
    "generate_pr_comment",
    "generate_report",
    "EXCEPTION_OVERHEAD",
    "TIMING",
    "log",
    "run_objdump",
    "run_size",
]
