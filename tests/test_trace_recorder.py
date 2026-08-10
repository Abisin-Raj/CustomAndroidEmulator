import os
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock
from devtools.trace.recorder import BootTraceRecorder, TraceStore, TraceEvent

class TestBootTraceRecorder(unittest.TestCase):
    def setUp(self):
        self.test_dir = tempfile.mkdtemp()

    def tearDown(self):
        shutil.rmtree(self.test_dir)

    def test_record_and_export_session(self):
        mock_session = MagicMock()
        recorder = BootTraceRecorder(mock_session)

        store = recorder.start_session("test-session-001")
        recorder.record(0, "EXECUTION", 0xFFFFFF8008080000, "start_kernel")
        recorder.record(1, "CPU_ONLINE", 0xFFFFFF800817A000, "secondary_start_kernel")

        self.assertEqual(len(store.events), 2)

        out_dir = os.path.join(self.test_dir, "traces", "test-session-001")
        recorder.stop_session(out_dir)

        self.assertTrue(os.path.exists(os.path.join(out_dir, "trace.json")))
        self.assertTrue(os.path.exists(os.path.join(out_dir, "metadata.json")))
        self.assertTrue(os.path.exists(os.path.join(out_dir, "timeline.md")))

if __name__ == "__main__":
    unittest.main()
