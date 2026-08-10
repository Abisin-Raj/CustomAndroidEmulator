import unittest
from unittest.mock import MagicMock
from devtools.common.events import MemoryAccessEvent
from devtools.common.watchpoints import MemoryWatchpointTracer

class TestMemoryWatchpoints(unittest.TestCase):
    def test_memory_access_event_format(self):
        event = MemoryAccessEvent(
            access_type="WRITE",
            cpu_id=0,
            pc=0xFFFFFF80080A1234,
            symbol="gcc_probe+0x84",
            va=0xFFFFFF800A931BE0,
            pa="gpa: 0x42931BE0",
            value=0xFFFFFF800808023C,
            registers={"X0": 0xFFFFFF800808023C},
            call_site="do_initcall+0x10",
            timestamp=1000.0
        )

        formatted = event.format_event()
        self.assertIn("[WRITE]", formatted)
        self.assertIn("vCPU0", formatted)
        self.assertIn("0xFFFFFF80080A1234", formatted)
        self.assertIn("gcc_probe+0x84", formatted)
        self.assertIn("0xFFFFFF800A931BE0", formatted)
        self.assertIn("gpa: 0x42931BE0", formatted)
        self.assertIn("0xFFFFFF800808023C", formatted)
        self.assertIn("do_initcall+0x10", formatted)

    def test_watchpoint_tracer_add_remove(self):
        mock_session = MagicMock()
        mock_session.gdb.set_breakpoint.return_value = True
        mock_session.gdb.remove_breakpoint.return_value = True

        tracer = MemoryWatchpointTracer(mock_session)
        target_va = 0xFFFFFF800A931BE0

        # Add watchpoint
        success = tracer.add_watchpoint(target_va, size=8, access_type="WRITE")
        self.assertTrue(success)
        self.assertIn(target_va, tracer.active_watchpoints)
        mock_session.gdb.set_breakpoint.assert_called_with(target_va, bp_type=2, size=8)

        # Remove watchpoint
        rem_success = tracer.remove_watchpoint(target_va)
        self.assertTrue(rem_success)
        self.assertNotIn(target_va, tracer.active_watchpoints)
        mock_session.gdb.remove_breakpoint.assert_called_with(target_va, bp_type=2, size=8)

if __name__ == "__main__":
    unittest.main()
