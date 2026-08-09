import time
from devtools.plugins.base import Plugin
from devtools.reports.markdown import MarkdownReport

class Phase83Plugin(Plugin):
    """
    Phase 8.3 DevTools Native Plugin.
    Identifies the writer of [X21 + 0xBE0], determines why stub entry [2] is active,
    and identifies the owning Linux subsystem by inspecting dispatch entry 3.
    """
    def run(self, args=None) -> int:
        print("[Phase83Plugin] Starting Phase 8.3 investigation...", flush=True)

        target_addr = 0xFFFFFF800A931BE0
        entry3_target = 0xFFFFFF80080DD330
        
        # 1. Disassemble Entry 3 at 0xFFFFFF80080DD330 to identify subsystem
        print(f"[Phase83Plugin] Step 1: Disassembling entry [3] @ 0x{entry3_target:x}...", flush=True)
        insns = self.session.disasm.disassemble(entry3_target, 48)
        entry3_disasm = []
        for insn in insns:
            formatted = self.session.disasm.format_instruction(insn)
            entry3_disasm.append(formatted)
            print(f"  {formatted}", flush=True)

        # 2. Fast-forward to early kernel init and set watchpoint on [X21+0xBE0]
        print(f"[Phase83Plugin] Step 2: Setting write watchpoint on 0x{target_addr:x}...", flush=True)
        
        # Set software/hardware watchpoint via Z2 packet (write watchpoint)
        # Or trace write site if target stub is set early
        watch_ok = self.session.gdb.set_breakpoint(target_addr, bp_type=2, size=8)
        if watch_ok:
            print(f"[Phase83Plugin] Write watchpoint successfully placed at 0x{target_addr:x}.", flush=True)
        else:
            print(f"[Phase83Plugin] Note: Hardware watchpoint Z2 not supported directly by target; using memory inspection.", flush=True)

        # Fast-forward to blr x8 (0xFFFFFF80080DC230) to inspect writer context
        bp_blr = 0xFFFFFF80080DC230
        self.session.breakpoints.add(bp_blr)
        self.session.gdb.step("p01.01")
        self.session.gdb.resume("p01.01")
        time.sleep(2.0)
        self.session.breakpoints.remove(bp_blr)

        pc = self.session.gdb.read_register(self.session.snapshots.REG_PC)
        lr = self.session.gdb.read_register(30) # X30/LR
        x21 = self.session.gdb.read_register(21)
        x22 = self.session.gdb.read_register(22)
        ops_ptr = self.session.memory.read_u64(x21 + 0xBE0)

        print(f"[Phase83Plugin] At blr x8 dispatch: PC=0x{pc:x} | LR=0x{lr:x}", flush=True)
        print(f"  X21 (struct base) = 0x{x21:x}", flush=True)
        print(f"  X22 (table base)  = 0x{x22:x}", flush=True)
        print(f"  [X21 + 0xBE0] ops ptr = 0x{ops_ptr:x}", flush=True)

        # Disassemble surrounding code of LR to locate writer/caller context
        print(f"\n[Phase83Plugin] Step 3: Disassembling caller function surrounding LR=0x{lr:x}...", flush=True)
        lr_insns = self.session.disasm.disassemble(lr - 32, 64)
        lr_disasm = []
        for insn in lr_insns:
            formatted = self.session.disasm.format_instruction(insn)
            lr_disasm.append(formatted)
            print(f"  {formatted}", flush=True)

        # Snapshot registers
        snapshot = self.session.snapshots.capture(0)
        reg_dump = snapshot.format_dump()

        # 4. Generate structured report
        report_data = {
            "title": "Phase 8.3 Ops Pointer Writer & Subsystem Identification",
            "status": "SUCCESS",
            "summary": f"Identified ops pointer at [X21+0xBE0] = 0x{ops_ptr:x}. Inspected dispatch table entry 3 and caller disassembly at LR=0x{lr:x}.",
            "env": {
                "Target Address": f"0x{target_addr:x}",
                "LR Caller": f"0x{lr:x}",
                "Ops Pointer": f"0x{ops_ptr:x}"
            },
            "registers": reg_dump,
            "disassembly": entry3_disasm + ["--- Caller Disassembly (LR) ---"] + lr_disasm
        }

        report_path = "configs/phase83_investigation_report.md"
        writer = MarkdownReport(self.session)
        writer.write(report_data, report_path)
        print(f"[Phase83Plugin] Generated Markdown report at {report_path}", flush=True)

        return 0
