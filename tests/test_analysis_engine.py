import os
import json
import shutil
import tempfile
import unittest
from devtools.analysis.engine import UnifiedAnalysisEngine, TraceAnalysisResult

class TestUnifiedAnalysisEngine(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_offline_trace_analysis(self):
        trace_json = os.path.join(self.test_dir, "trace.json")
        sample_events = [
            {"cpu_id": 0, "event_type": "EXECUTION", "pc": 0xFFFFFF8008080000, "symbol": "start_kernel"},
            {"cpu_id": 0, "event_type": "BLOCKER_DETECTED", "pc": 0xFFFFFF80080DC230, "symbol": "do_initcalls", "metadata": {"node": "qcom,gcc-sm6150"}}
        ]
        with open(trace_json, "w", encoding="utf-8") as f:
            json.dump(sample_events, f)

        engine = UnifiedAnalysisEngine()
        result = engine.analyze_trace_directory(self.test_dir)

        self.assertEqual(result.stall_function, "do_initcalls")
        self.assertEqual(result.missing_dt_node, "qcom,gcc-sm6150")
        self.assertEqual(result.confidence, 0.95)

        report = result.format_report()
        self.assertIn("UNIFIED BOOT TRACE ANALYSIS REPORT", report)
        self.assertIn("qcom,gcc-sm6150", report)

if __name__ == "__main__":
    unittest.main()
