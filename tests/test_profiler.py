import unittest
from unittest.mock import MagicMock
from devtools.profiler.execution import ExecutionProfiler, FunctionProfileData, HotLoopData

class TestExecutionProfiler(unittest.TestCase):
    def test_record_calls_and_steps(self):
        mock_session = MagicMock()
        profiler = ExecutionProfiler(mock_session)

        profiler.record_call("do_initcalls", 0xFFFFFF80080DC200)
        profiler.record_step("do_initcalls", 0xFFFFFF80080DC214, steps=100)

        self.assertIn("do_initcalls", profiler.profiles)
        prof = profiler.profiles["do_initcalls"]
        self.assertEqual(prof.call_count, 1)
        self.assertEqual(prof.step_count, 100)

    def test_detect_hot_loops(self):
        mock_session = MagicMock()
        mock_session.symbols.resolve.return_value = {"name": "do_initcalls", "address": 0xFFFFFF80080DC200, "offset": 0x18}
        profiler = ExecutionProfiler(mock_session)

        stall_pc = 0xFFFFFF80080DC218
        for _ in range(150):
            profiler.pc_history.append(stall_pc)

        loops = profiler.detect_hot_loops(threshold=100)
        self.assertEqual(len(loops), 1)
        self.assertEqual(loops[0].start_pc, stall_pc)
        self.assertEqual(loops[0].iterations, 150)
        self.assertEqual(loops[0].function_name, "do_initcalls")

    def test_profile_matrix_formatting(self):
        mock_session = MagicMock()
        profiler = ExecutionProfiler(mock_session)
        profiler.record_call("do_initcalls", 0xFFFFFF80080DC200)
        profiler.record_step("do_initcalls", 0xFFFFFF80080DC214, steps=500)

        matrix = profiler.format_profile_matrix()
        self.assertIn("KERNEL EXECUTION PROFILE", matrix)
        self.assertIn("do_initcalls", matrix)
        self.assertIn("500", matrix)

if __name__ == "__main__":
    unittest.main()
