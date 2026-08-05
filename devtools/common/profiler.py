import time

class FunctionProfiler:
    """
    Profiles execution metrics (instruction counts, time spent, call frequencies)
    at the function boundary level.
    """
    def __init__(self, session):
        self.session = session
        self.function_calls = {}  # name -> { "calls": int, "instructions": int, "start_time": float, "total_time": float }
        self.active_functions = []  # call stack of (name, start_time, instruction_start_count)

    def record_enter(self, name: str, step_count: int):
        """Records entry into a function."""
        now = time.time()
        self.active_functions.append((name, now, step_count))
        stats = self.function_calls.setdefault(name, {
            "calls": 0, "instructions": 0, "total_time": 0.0
        })
        stats["calls"] += 1

    def record_exit(self, name: str, step_count: int):
        """Records exit from a function."""
        # Find matching function entry in active stack
        for idx in reversed(range(len(self.active_functions))):
            entry_name, start_time, start_step = self.active_functions[idx]
            if entry_name == name:
                now = time.time()
                elapsed_time = now - start_time
                elapsed_steps = step_count - start_step
                
                stats = self.function_calls[name]
                stats["instructions"] += elapsed_steps
                stats["total_time"] += elapsed_time
                
                self.active_functions.pop(idx)
                break

    def get_summary(self) -> dict:
        """Returns the accumulated profile results."""
        return self.function_calls
