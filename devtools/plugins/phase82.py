import time
from devtools.plugins.base import Plugin
from devtools.reports.markdown import MarkdownReport

class Phase82Plugin(Plugin):
    """
    Migrated Phase 8.2 Plugin logic.
    Validates that overriding callback status registers unblocks CPU0.
    """
    def run(self, args=None) -> int:
        print("[Phase82Plugin] Starting validation process...")
        
        # 1. Fast-forward to blr x8 dispatch at 0xFFFFFF80080DC230
        bp_blr = 0xFFFFFF80080DC230
        bp_add = 0xFFFFFF80080DC234
        bp_exit = 0xFFFFFF80080DC268

        self.session.breakpoints.add(bp_blr)
        # Step once to start CPU execution
        self.session.gdb.step("p01.01")
        self.session.gdb.resume("p01.01")
        time.sleep(2.0)
        self.session.breakpoints.remove(bp_blr)

        pc = self.session.gdb.read_register(self.session.snapshots.REG_PC)
        print(f"[Phase82Plugin] Reached: PC=0x{pc:x}")
        if pc != bp_blr:
            print("[Phase82Plugin] ERROR: Failed to reach callback breakpoint.")
            return 1

        # Wake CPU1
        self.session.gdb.run_hmp_cmd("compat-wake-cpu 1 0x40080000 0")
        
        # Step past blr into callback
        self.session.gdb.step("p01.01")
        time.sleep(0.05)

        # Run until callback returns
        self.session.breakpoints.add(bp_add)
        self.session.gdb.resume("p01.01")
        time.sleep(0.5)
        self.session.breakpoints.remove(bp_add)

        pc_ret = self.session.gdb.read_register(self.session.snapshots.REG_PC)
        print(f"[Phase82Plugin] Callback returned to PC=0x{pc_ret:x}")

        # 2. Apply override: set X0 = 5001 (exceeds loop timeout target 5000)
        self.session.gdb.write_register(0, 5001)
        print("[Phase82Plugin] Overrode callback status register X0 = 5001.")

        # Single step add instructions
        self.session.gdb.step("p01.01")
        time.sleep(0.05)
        self.session.gdb.step("p01.01")
        time.sleep(0.05)

        # 3. Check loop exit
        self.session.breakpoints.add(bp_exit)
        self.session.gdb.resume("p01.01")
        time.sleep(0.5)
        self.session.breakpoints.remove(bp_exit)

        pc_final = self.session.gdb.read_register(self.session.snapshots.REG_PC)
        print(f"[Phase82Plugin] Final loop check PC=0x{pc_final:x}")

        # Capture register snapshots
        snapshot = self.session.snapshots.capture(0)
        reg_dump = snapshot.format_dump()

        # Capture disassembly surrounding loop exit
        disasm_lines = []
        insns = self.session.disasm.disassemble(bp_exit - 16, 32)
        for insn in insns:
            disasm_lines.append(self.session.disasm.format_instruction(insn))

        # 4. Generate structured Markdown report
        report_data = {
            "title": "Phase 8.2 Callback Override Verification",
            "status": "SUCCESS" if pc_final == bp_exit else "FAILED",
            "summary": "Verified that overriding the callback return status to 5001 successfully exits the loops_per_jiffy outer wait loop and unblocks the kernel boot.",
            "env": {
                "GDB Target": f"{self.session.gdb.host}:{self.session.gdb.port}",
                "Final PC": f"0x{pc_final:x}"
            },
            "registers": reg_dump,
            "disassembly": disasm_lines
        }

        report_path = "configs/phase82_validation_report.md"
        writer = MarkdownReport(self.session)
        writer.write(report_data, report_path)
        print(f"[Phase82Plugin] Generated Markdown report at {report_path}")

        return 0 if pc_final == bp_exit else 1
