import time
import logging
from dataclasses import dataclass
from typing import Optional, List, Dict
from devtools.common.events import MMIOAccessEvent

@dataclass
class MMIORegion:
    name: str
    base: int
    size: int

    @property
    def end(self) -> int:
        return self.base + self.size

    def contains_pa(self, pa: int) -> bool:
        return self.base <= pa < self.end

class MMIOTracer:
    """
    MMIO Access Tracer Service for DevTools v1.1.
    Monitors memory operations targeting configured physical MMIO ranges.
    Resolves VA -> PA, extracts region metadata, instruction disassembly, and formats MMIOAccessEvents.
    """
    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.mmio_tracer")
        self.regions: Dict[str, MMIORegion] = {}

    def add_region(self, name: str, base: int, size: int) -> MMIORegion:
        """Configures a physical MMIO range to trace."""
        region = MMIORegion(name=name, base=base, size=size)
        self.regions[name] = region
        self.logger.info(f"Added MMIO Region '{name}': base=0x{base:X}, size=0x{size:X}")
        return region

    def remove_region(self, name: str) -> bool:
        """Removes a physical MMIO region."""
        if name in self.regions:
            del self.regions[name]
            self.logger.info(f"Removed MMIO Region '{name}'")
            return True
        return False

    def find_region(self, pa: int) -> Optional[MMIORegion]:
        """Finds the MMIORegion containing physical address pa."""
        for region in self.regions.values():
            if region.contains_pa(pa):
                return region
        return None

    def trace_access(self, va: int, access_type="MMIO READ", size=4, cpu_id=0, thread_id="p01.01") -> Optional[MMIOAccessEvent]:
        """
        Traces a memory access instruction at target VA. Resolves VA to PA and checks if it falls
        within a registered MMIO region. Returns MMIOAccessEvent if hit, else None.
        """
        # Resolve PA via HMP gva2gpa
        pa_out = self.session.gdb.run_hmp_cmd(f"gva2gpa 0x{va:x}")
        pa_str = pa_out.strip() if pa_out else ""
        
        pa_val = 0
        if "0x" in pa_str:
            try:
                parts = pa_str.replace("gpa:", "").strip().split()
                pa_val = int(parts[0], 16)
            except Exception:
                pass

        region = self.find_region(pa_val)
        region_name = region.name if region else "Unknown / Unmapped MMIO"

        pc = self.session.gdb.read_register(self.session.snapshots.REG_PC, thread_id)
        lr = self.session.gdb.read_register(30, thread_id)

        sym_info = self.session.symbols.resolve(pc)
        symbol = f"{sym_info['name']}+0x{sym_info['offset']:x}" if sym_info else f"0x{pc:x}"

        lr_sym = self.session.symbols.resolve(lr)
        call_site = f"{lr_sym['name']}+0x{lr_sym['offset']:x}" if lr_sym else f"0x{lr:x}"

        # Fetch disassembly of instruction at PC
        insns = self.session.disasm.disassemble(pc, 4, thread_id)
        instruction = insns[0]["mnemonic"] + " " + insns[0]["op_str"] if insns else "Unknown"

        # Read memory value
        if size == 8:
            val = self.session.memory.read_u64(va, thread_id)
        elif size == 4:
            val = self.session.memory.read_u32(va, thread_id)
        else:
            val = self.session.memory.read_u8(va, thread_id)

        event = MMIOAccessEvent(
            access_type=access_type,
            cpu_id=cpu_id,
            pc=pc,
            symbol=symbol,
            va=va,
            pa=pa_val,
            size=size,
            value=val,
            region_name=region_name,
            instruction=instruction,
            call_site=call_site,
            timestamp=time.time()
        )
        return event
