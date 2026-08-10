import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

@dataclass
class FunctionProfileData:
    name: str
    call_count: int = 0
    step_count: int = 0
    total_time_sec: float = 0.0
    start_pc: int = 0

@dataclass
class HotLoopData:
    start_pc: int
    end_pc: int
    iterations: int
    function_name: str
    classification: str

class ExecutionProfiler:
    """
    Kernel Execution Profiler for DevTools v1.1.
    Tracks function calls, instruction step counts, execution durations, hot PC loops, and stalls.
    """
    def __init__(self, session):
        self.session = session
        self.profiles: Dict[str, FunctionProfileData] = {}
        self.hot_loops: List[HotLoopData] = []
        self.pc_history: List[int] = []

    def record_call(self, fn_name: str, pc: int):
        """Records a function call entry."""
        if fn_name not in self.profiles:
            self.profiles[fn_name] = FunctionProfileData(name=fn_name, start_pc=pc)
        profile = self.profiles[fn_name]
        profile.call_count += 1

    def record_step(self, fn_name: str, pc: int, steps: int = 1):
        """Records instruction steps executed in a function."""
        if fn_name not in self.profiles:
            self.profiles[fn_name] = FunctionProfileData(name=fn_name, start_pc=pc)
        profile = self.profiles[fn_name]
        profile.step_count += steps
        self.pc_history.append(pc)

    def detect_hot_loops(self, threshold=100) -> List[HotLoopData]:
        """Scans PC history for repeated loop patterns."""
        if len(self.pc_history) < 10:
            return self.hot_loops

        counts: Dict[int, int] = {}
        for pc in self.pc_history:
            counts[pc] = counts.get(pc, 0) + 1

        for pc, count in counts.items():
            if count >= threshold:
                sym_info = self.session.symbols.resolve(pc) if hasattr(self.session, "symbols") else None
                fn_name = sym_info["name"] if sym_info else f"0x{pc:X}"
                
                # Avoid duplicates
                if not any(hl.start_pc == pc for hl in self.hot_loops):
                    self.hot_loops.append(HotLoopData(
                        start_pc=pc,
                        end_pc=pc + 4,
                        iterations=count,
                        function_name=fn_name,
                        classification="Busy-wait / retry loop (calibration stall)"
                    ))
        return self.hot_loops

    def format_profile_matrix(self) -> str:
        out = []
        out.append("KERNEL EXECUTION PROFILE")
        out.append(f"{'Function':<30} {'Calls':<10} {'Steps':<12} {'Time (s)':<10}")
        out.append("-" * 65)

        for prof in self.profiles.values():
            out.append(f"{prof.name:<30} {prof.call_count:<10} {prof.step_count:<12} {prof.total_time_sec:<10.2f}")

        if self.hot_loops:
            out.append("\nHOT LOOPS & STALLS DETECTED")
            for hl in self.hot_loops:
                out.append(f"  PC: 0x{hl.start_pc:016X} | Iterations: {hl.iterations} | Function: {hl.function_name}")
                out.append(f"  Classification: {hl.classification}")

        return "\n".join(out)
