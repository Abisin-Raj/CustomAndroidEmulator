import logging

class RegisterSnapshot:
    """
    Holds a captured snapshot of general-purpose and system EL1 control registers.
    """
    def __init__(self, cpu_id: int):
        self.cpu_id = cpu_id
        # General Purpose Registers
        self.regs = {}  # "X0" -> int
        self.sp = 0
        self.pc = 0
        self.lr = 0
        self.pstate = 0
        
        # System registers
        self.sctlr = 0
        self.ttbr0 = 0
        self.ttbr1 = 0
        self.tcr = 0
        self.mair = 0
        self.esr = 0
        self.far = 0
        self.current_el = 0

    def format_dump(self) -> str:
        """Formats the snapshot into a readable text report."""
        out = []
        out.append(f"=== CPU {self.cpu_id} Register Snapshot ===")
        # Print general registers in 4-columns
        gpr_lines = []
        for i in range(0, 31, 4):
            cols = []
            for r in range(i, min(i+4, 31)):
                val = self.regs.get(f"X{r}", 0)
                cols.append(f"X{r:<2}=0x{val:016x}")
            gpr_lines.append("  " + "  ".join(cols))
        out.extend(gpr_lines)
        out.append(f"  SP =0x{self.sp:016x}   PC =0x{self.pc:016x}   LR =0x{self.lr:016x}")
        out.append(f"  PSTATE=0x{self.pstate:08x}  CurrentEL={self.current_el}")
        out.append("--- System EL1 Control Registers ---")
        out.append(f"  SCTLR =0x{self.sctlr:016x}   TTBR0=0x{self.ttbr0:016x}")
        out.append(f"  TTBR1 =0x{self.ttbr1:016x}   TCR  =0x{self.tcr:016x}")
        out.append(f"  MAIR  =0x{self.mair:016x}   ESR  =0x{self.esr:016x}   FAR=0x{self.far:016x}")
        return "\n".join(out)


class RegisterSnapshotEngine:
    """
    Engine to capture RegisterSnapshots via GDB RSP interface.
    Uses ARM64 XML target descriptor mapping values to extract EL1 registers.
    """
    # GDB register number mappings for standard aarch64 target
    REG_X0 = 0
    REG_SP = 31
    REG_PC = 32
    REG_PSTATE = 33

    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.snapshots")

    def capture(self, cpu_id: int) -> RegisterSnapshot:
        """Captures a snapshot of the registers for the specified CPU ID (0 or 1)."""
        thread_id = f"p01.0{cpu_id + 1}"
        snapshot = RegisterSnapshot(cpu_id)

        # 1. Capture General Purpose Registers
        for r in range(31):
            snapshot.regs[f"X{r}"] = self.session.gdb.read_register(r, thread_id)
        snapshot.sp = self.session.gdb.read_register(self.REG_SP, thread_id)
        snapshot.pc = self.session.gdb.read_register(self.REG_PC, thread_id)
        snapshot.pstate = self.session.gdb.read_register(self.REG_PSTATE, thread_id)
        snapshot.lr = snapshot.regs["X30"]

        # 2. Capture System Registers via HMP info registers if direct reads are unmapped in GDB xml
        # HMP "info registers -a" or direct print commands
        hmp_data = self.session.gdb.run_hmp_cmd(f"info registers -a")
        self._parse_hmp_sys_regs(hmp_data, snapshot)

        return snapshot

    def _parse_hmp_sys_regs(self, hmp_data: str, snapshot: RegisterSnapshot):
        """Parses SCTLR, TTBR0/1, TCR, MAIR, ESR, FAR from QEMU monitor output."""
        for line in hmp_data.splitlines():
            line = line.strip()
            # SCTLR_EL1
            if "SCTLR_EL1" in line:
                parts = line.split("SCTLR_EL1")[-1].split()
                if parts:
                    snapshot.sctlr = int(parts[0], 16)
            # TTBR0_EL1
            if "TTBR0_EL1" in line:
                parts = line.split("TTBR0_EL1")[-1].split()
                if parts:
                    snapshot.ttbr0 = int(parts[0], 16)
            # TTBR1_EL1
            if "TTBR1_EL1" in line:
                parts = line.split("TTBR1_EL1")[-1].split()
                if parts:
                    snapshot.ttbr1 = int(parts[0], 16)
            # TCR_EL1
            if "TCR_EL1" in line:
                parts = line.split("TCR_EL1")[-1].split()
                if parts:
                    snapshot.tcr = int(parts[0], 16)
            # MAIR_EL1
            if "MAIR_EL1" in line:
                parts = line.split("MAIR_EL1")[-1].split()
                if parts:
                    snapshot.mair = int(parts[0], 16)
            # ESR_EL1
            if "ESR_EL1" in line:
                parts = line.split("ESR_EL1")[-1].split()
                if parts:
                    snapshot.esr = int(parts[0], 16)
            # FAR_EL1
            if "FAR_EL1" in line:
                parts = line.split("FAR_EL1")[-1].split()
                if parts:
                    snapshot.far = int(parts[0], 16)
            # CurrentEL
            if "CurrentEL" in line or "currentel" in line.lower():
                # Extract value
                try:
                    parts = line.replace("=", " ").split()
                    for idx, p in enumerate(parts):
                        if "currentel" in p.lower() and idx + 1 < len(parts):
                            snapshot.current_el = int(parts[idx+1], 16)
                except Exception:
                    pass
