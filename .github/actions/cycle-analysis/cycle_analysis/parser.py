"""
ARM disassembly parser.

Transforms raw arm-none-eabi-objdump -d -C output into a list of Function
objects with per-instruction cycle estimates.
"""

from __future__ import annotations

import re
from typing import Optional

from cycle_analysis.models import Function, Instruction
from cycle_analysis.timing_model import classify_instruction, get_timing


def parse_disassembly(objdump_output: str) -> list[Function]:
    functions: list[Function] = []
    current_fn: Optional[Function] = None

    fn_pattern = re.compile(r"^([0-9a-fA-F]+)\s+<(.+?)>:$")
    instr_pattern = re.compile(
        r"^\s*([0-9a-fA-F]+):\s+([0-9a-fA-F ]+?)\s{2,}(\S+)\s*(.*?)$"
    )
    call_target = re.compile(r"<(.+?)>")

    for line in objdump_output.splitlines():
        fn_match = fn_pattern.match(line)
        if fn_match:
            if current_fn:
                _finalize(current_fn)
                functions.append(current_fn)
            addr = int(fn_match.group(1), 16)
            name = fn_match.group(2)
            current_fn = Function(name=name, demangled=name, start_addr=addr)
            continue

        if current_fn is None:
            continue

        instr_match = instr_pattern.match(line)
        if instr_match:
            addr = int(instr_match.group(1), 16)
            raw_bytes = instr_match.group(2).strip()
            mnemonic = instr_match.group(3).strip()
            operands = instr_match.group(4).strip()
            byte_count = len(raw_bytes.replace(" ", "")) // 2
            min_c, max_c = get_timing(mnemonic)

            instr = Instruction(
                address=addr, raw_bytes=raw_bytes, mnemonic=mnemonic,
                operands=operands, size=byte_count,
                min_cycles=min_c, max_cycles=max_c,
            )
            current_fn.instructions.append(instr)

            if mnemonic.lower() in ("bl", "blx", "b", "bx") or mnemonic.lower().startswith("bl"):
                ct = call_target.search(operands)
                if ct:
                    current_fn.calls.append(ct.group(1))

    if current_fn:
        _finalize(current_fn)
        functions.append(current_fn)

    return functions


def _finalize(fn: Function) -> None:
    fn.instruction_count = len(fn.instructions)
    fn.total_min_cycles = sum(i.min_cycles for i in fn.instructions)
    fn.total_max_cycles = sum(i.max_cycles for i in fn.instructions)
    fn.code_size = sum(i.size for i in fn.instructions)
    if fn.instructions:
        fn.end_addr = fn.instructions[-1].address + fn.instructions[-1].size
    for instr in fn.instructions:
        cat = classify_instruction(instr.mnemonic)
        if cat == "fpu":
            fn.fpu_ops += 1
        elif cat == "load_store":
            fn.load_store_ops += 1
        elif cat == "branch":
            fn.branch_ops += 1
        elif cat == "mul_div":
            fn.mul_div_ops += 1
        else:
            fn.alu_ops += 1
