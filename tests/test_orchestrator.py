import unittest
from unittest.mock import MagicMock
from devtools.orchestration.investigator import InvestigationOrchestrator, InvestigationResult

class TestInvestigationOrchestrator(unittest.TestCase):
    def test_investigation_result_formatting(self):
        result = InvestigationResult(
            session_id="boot-20260810-001",
            confidence=0.95
        )
        report = result.format_report()

        self.assertIn("DEVTOOLS INVESTIGATION", report)
        self.assertIn("boot-20260810-001", report)
        self.assertIn("vCPU0 ONLINE", report)
        self.assertIn("vCPU1 ONLINE", report)
        self.assertIn("NEXT INVESTIGATION TARGET", report)
        self.assertIn("Qualcomm GCC platform device", report)
        self.assertIn("Recommended Next Steps:", report)

    def test_orchestrator_execution(self):
        mock_session = MagicMock()
        mock_session.gdb.read_register.return_value = 0xFFFFFF80080DC230

        orchestrator = InvestigationOrchestrator(mock_session)
        result = orchestrator.investigate()

        self.assertEqual(result.stall_pc, 0xFFFFFF80080DC230)
        self.assertEqual(result.confidence, 0.95)

if __name__ == "__main__":
    unittest.main()
