import re
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple

@dataclass
class FunctionBoundary:
    name: str
    start_addr: int
    end_addr: int
    size: int

@dataclass
class CrossReference:
    source_pc: int
    instruction: str
    mnemonic: str
    op_str: str
    ref_type: str  # "READ" | "WRITE" | "BRANCH" | "IMMEDIATE"

class SymbolExplorer:
    """
    Kernel Symbol & Structure Explorer for DevTools v1.1.
    Provides symbol lookup, function boundary resolution, disassembly navigation,
    structure offset inspection (e.g. [X21 + 0xBE0]), and disassembly cross-reference (xref) scanning.
    """
    def __init__(self, session):
        self.session = session

    def lookup(self, name_or_addr) -> Optional[dict]:
        """Resolves symbol name or hexadecimal virtual address."""
        if isinstance(name_or_addr, str):
            if name_or_addr.startswith("0x") or name_or_addr.startswith("0X"):
                addr = int(name_or_addr, 16)
                return self.session.symbols.resolve(addr)
            else:
                addr = self.session.symbols.lookup(name_or_addr)
                if addr:
                    return {"name": name_or_addr, "address": addr, "offset": 0}
        elif isinstance(name_or_addr, int):
            return self.session.symbols.resolve(name_or_addr)
        return None

    def find_xrefs(self, target_offset_or_addr: str, insn_stream: List[dict]) -> List[CrossReference]:
        """
        Scans instruction disassembly stream for cross-references to a target address or offset string.
        (e.g., "#0xbe0", "0x8080230", "0x80dd330")
        """
        xrefs = []
        target_str = str(target_offset_or_addr).lower()

        for insn in insn_stream:
            mnemonic = insn["mnemonic"]
            op_str = insn["op_str"]
            pc = insn["address"]

            if target_str in op_str.lower() or target_str in mnemonic.lower():
                ref_type = "READ" if "ldr" in mnemonic else ("WRITE" if "str" in mnemonic else "BRANCH")
                xrefs.append(CrossReference(
                    source_pc=pc,
                    instruction=f"{mnemonic:<8} {op_str}",
                    mnemonic=mnemonic,
                    op_str=op_str,
                    ref_type=ref_type
                ))

        return xrefs

    def disassemble_symbol(self, name_or_addr, num_instructions=32, thread_id="p01.01") -> List[dict]:
        """Disassembles instruction stream starting at symbol or address."""
        info = self.lookup(name_or_addr)
        if not info:
            return []
        addr = info.get("address", 0)
        return self.session.disasm.disassemble(addr, num_instructions * 4, thread_id)
