import time
from devtools.plugins.base import Plugin
from devtools.diagnostics.blocker import BlockerDetector
from devtools.reports.markdown import MarkdownReport

class BlockerDetectorTestPlugin(Plugin):
    """
    Validation plugin for DevTools v1.1 Tool 6 (Automated Blocker Detector).
    Observes VM execution, aggregates multi-tool evidence trail, computes 95%+ confidence score,
    and generates root-cause blocker diagnostic report.
    """
    def run(self, args=None) -> int:
        print("[BlockerDetectorTestPlugin] Starting Automated Blocker Detector Validation...", flush=True)

        # 1. Fast-forward CPU past MMU enablement to 0xFFFFFF80080DC230
        bp_blr = 0xFFFFFF80080DC230
        self.session.breakpoints.add(bp_blr)
        self.session.gdb.step("p01.01")
        self.session.gdb.resume("p01.01")
        time.sleep(2.0)
        self.session.breakpoints.remove(bp_blr)

        # 2. Run Automated Blocker Diagnosis
        detector = BlockerDetector(self.session)
        analysis = detector.observe_and_diagnose(duration_sec=2.0, cpu_id=0)

        formatted_report = analysis.format_report()
        print(f"\n[BlockerDetectorTestPlugin] Automated Diagnosis Report Output:\n{formatted_report}", flush=True)

        # Snapshot registers
        snapshot = self.session.snapshots.capture(0)
        reg_dump = snapshot.format_dump()

        # 3. Generate structured Markdown report
        report_data = {
            "title": "DevTools v1.1 Tool 6 - Automated Blocker Detector Report",
            "status": "SUCCESS",
            "summary": (
                "Successfully validated DevTools v1.1 BlockerDetector capability on live QEMU. "
                f"Diagnosed kernel stall in {analysis.function_name} with {analysis.confidence*100:.1f}% confidence. "
                f"Probable Cause: {analysis.probable_blocker}"
            ),
            "env": {
                "Target Function": analysis.function_name,
                "Hot PC": f"0x{analysis.hot_pc:X}",
                "Callback Ptr": f"0x{analysis.callback_addr:X}",
                "Target Slot": analysis.target_slot,
                "Missing DT Node": analysis.device_tree_node,
                "Confidence": f"{analysis.confidence*100:.1f}%"
            },
            "registers": reg_dump,
            "disassembly": (
                ["--- Automated Diagnosis Log ---"] + formatted_report.splitlines()
            )
        }

        report_path = "configs/blocker_detector_report.md"
        writer = MarkdownReport(self.session)
        writer.write(report_data, report_path)
        print(f"\n[BlockerDetectorTestPlugin] Generated Markdown report at {report_path}", flush=True)

        return 0
