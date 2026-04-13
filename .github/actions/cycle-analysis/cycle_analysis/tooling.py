"""
Shell-tool helpers: logging, objdump invocation, and size-tool invocation.
"""

from __future__ import annotations

import subprocess
import sys


def log(msg: str) -> None:
    print(msg, file=sys.stderr)


def run_objdump(elf: str, tool: str = "arm-none-eabi-objdump") -> str:
    cmd = [tool, "-d", "-C", elf]
    log(f"Running: {' '.join(cmd)}")
    return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout


def run_size(elf: str, tool: str = "arm-none-eabi-size") -> str:
    cmd = [tool, "-A", elf]
    try:
        return subprocess.run(cmd, capture_output=True, text=True, check=True).stdout
    except (subprocess.CalledProcessError, FileNotFoundError):
        return ""
