import time
from dataclasses import dataclass, field
from typing import Dict, List, Optional

@dataclass
class SMPEvent:
    timestamp: float
    cpu_id: int
    pc: int
    symbol_name: str
    milestone_name: Optional[str] = None
    status: str = "RUNNING"  # "RUNNING" | "RESET" | "WAITING" | "ONLINE" | "STALLED"

@dataclass
class CPUExecutionState:
    cpu_id: int
    current_pc: int = 0
    current_symbol: str = "reset"
    last_active_time: float = 0.0
    is_online: bool = False
    is_stalled: bool = False
    milestones_reached: List[str] = field(default_factory=list)

class SMPTimelineEngine:
    """
    SMP Timeline Analyzer for DevTools v1.1.
    Tracks multi-vCPU execution states across vCPU0 and vCPU1, captures CPU bringup milestones,
    merges multi-vCPU event streams into a unified chronological timeline, and identifies stalled CPUs.
    """
    KNOWN_MILESTONES = {
        "secondary_start_kernel": "vCPU Secondary Entry",
        "start_kernel": "Kernel Start",
        "__cpu_up": "vCPU Bring-Up Trigger",
        "smp_init": "SMP Initialization",
        "cpu_startup_entry": "CPU Idle Loop Entry",
        "do_initcalls": "Initcall Loop Entry"
    }


    def __init__(self, session):
        self.session = session
        self.events: List[SMPEvent] = []
        self.cpus: Dict[int, CPUExecutionState] = {
            0: CPUExecutionState(cpu_id=0, is_online=True),
            1: CPUExecutionState(cpu_id=1, is_online=False)
        }

    def record_event(self, cpu_id: int, pc: int, symbol_name: str, status="RUNNING") -> SMPEvent:
        """Records an execution event for a specific vCPU."""
        if cpu_id not in self.cpus:
            self.cpus[cpu_id] = CPUExecutionState(cpu_id=cpu_id)

        cpu_state = self.cpus[cpu_id]
        cpu_state.current_pc = pc
        cpu_state.current_symbol = symbol_name
        cpu_state.last_active_time = time.time()

        milestone = None
        for sym_pattern, name in self.KNOWN_MILESTONES.items():
            if sym_pattern in symbol_name:
                milestone = name
                if name not in cpu_state.milestones_reached:
                    cpu_state.milestones_reached.append(name)
                if "Secondary" in name or "Bring-Up" in name or "Idle" in name:
                    cpu_state.is_online = True
                break

        evt = SMPEvent(
            timestamp=time.time(),
            cpu_id=cpu_id,
            pc=pc,
            symbol_name=symbol_name,
            milestone_name=milestone,
            status=status
        )
        self.events.append(evt)
        return evt

    def detect_cpu_stalls(self, threshold_sec=2.0) -> List[CPUExecutionState]:
        """Identifies any vCPU that has stopped making progress."""
        stalled = []
        now = time.time()
        for cpu_state in self.cpus.values():
            if cpu_state.is_online and (now - cpu_state.last_active_time > threshold_sec):
                cpu_state.is_stalled = True
                stalled.append(cpu_state)
        return stalled

    def format_timeline_report(self) -> str:

        out = []
        out.append("SMP BOOT TIMELINE & MULTI-vCPU STATUS")
        out.append("=" * 60)
        out.append(f"{'TIME (s)':<10} {'CPU0 EXECUTION':<30} {'CPU1 EXECUTION':<30}")
        out.append("-" * 70)

        cpu0_symbol = self.cpus[0].current_symbol if 0 in self.cpus else "reset"
        cpu1_symbol = self.cpus[1].current_symbol if 1 in self.cpus else "reset"
        out.append(f"{0.000:<10.3f} {cpu0_symbol:<30} {cpu1_symbol:<30}")

        out.append("\nvCPU STATUS MATRIX:")
        for cpu_id, state in sorted(self.cpus.items()):
            online_str = "ONLINE" if state.is_online else "RESET / OFFLINE"
            stalled_str = "STALLED" if state.is_stalled else "ACTIVE"
            out.append(f"  vCPU{cpu_id}: Status={online_str:<15} State={stalled_str:<10} PC=0x{state.current_pc:016X} ({state.current_symbol})")
            if state.milestones_reached:
                out.append(f"          Milestones: {', '.join(state.milestones_reached)}")

        return "\n".join(out)
