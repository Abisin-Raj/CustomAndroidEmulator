import unittest
from unittest.mock import MagicMock
from devtools.smp.timeline import SMPTimelineEngine, SMPEvent, CPUExecutionState

class TestSMPTimeline(unittest.TestCase):
    def test_record_events_and_milestones(self):
        mock_session = MagicMock()
        engine = SMPTimelineEngine(mock_session)

        # Record vCPU0 event
        evt0 = engine.record_event(0, 0xFFFFFF8008080000, "start_kernel")
        self.assertEqual(evt0.cpu_id, 0)
        self.assertEqual(evt0.milestone_name, "Kernel Start")
        self.assertTrue(engine.cpus[0].is_online)

        # Record vCPU1 event
        evt1 = engine.record_event(1, 0xFFFFFF800817A000, "secondary_start_kernel")
        self.assertEqual(evt1.cpu_id, 1)
        self.assertEqual(evt1.milestone_name, "vCPU Secondary Entry")
        self.assertTrue(engine.cpus[1].is_online)

    def test_timeline_report_formatting(self):
        mock_session = MagicMock()
        engine = SMPTimelineEngine(mock_session)
        engine.record_event(0, 0xFFFFFF8008080000, "start_kernel")
        engine.record_event(1, 0xFFFFFF800817A000, "secondary_start_kernel")

        report = engine.format_timeline_report()
        self.assertIn("SMP BOOT TIMELINE", report)
        self.assertIn("vCPU0", report)
        self.assertIn("vCPU1", report)
        self.assertIn("secondary_start_kernel", report)

if __name__ == "__main__":
    unittest.main()
