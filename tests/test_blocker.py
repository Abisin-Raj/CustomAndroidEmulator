import unittest
from unittest.mock import MagicMock
from devtools.diagnostics.blocker import BlockerDetector, BlockerEvidence, BlockerAnalysis

class TestBlockerDetector(unittest.TestCase):
    def test_blocker_analysis_formatting(self):
        analysis = BlockerAnalysis(
            vcpu_id=0,
            function_name="do_initcalls()",
            hot_pc=0xFFFFFF80080DC230,
            stall_pattern="Repeated execution",
            iterations=1201,
            callback_addr=0xFFFFFF8008080238,
            target_slot="Slot 2",
            return_behavior="X0 = 0",
            device_tree_node="qcom,gcc-sm6150",
            mmio_range="0x00100000-0x002F0000",
            confidence=0.95,
            probable_blocker="Missing Qualcomm GCC platform device",
            evidences=[
                BlockerEvidence("ExecutionProfiler", "STALL", "Loop stall", 0.35),
                BlockerEvidence("DeviceTreeComparator", "MISSING_NODE", "virt.dtb lacks node", 0.35)
            ]
        )
        report = analysis.format_report()
        self.assertIn("KERNEL BLOCKER DETECTED", report)
        self.assertIn("vCPU0", report)
        self.assertIn("do_initcalls()", report)
        self.assertIn("0xFFFFFF80080DC230", report)
        self.assertIn("95.0%", report)

    def test_blocker_detector_aggregation(self):
        mock_session = MagicMock()
        mock_session.gdb.read_register.return_value = 0xFFFFFF80080DC230
        mock_session.memory.read_u64.return_value = 0xFFFFFF8008080238

        detector = BlockerDetector(mock_session)
        analysis = detector.observe_and_diagnose(duration_sec=1.0)
        self.assertGreaterEqual(analysis.confidence, 0.90)
        self.assertEqual(analysis.device_tree_node, "qcom,gcc-sm6150")

if __name__ == "__main__":
    unittest.main()
