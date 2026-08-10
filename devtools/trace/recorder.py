import os
import json
import time
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional

@dataclass
class TraceEvent:
    timestamp: float
    cpu_id: int
    event_type: str  # EXECUTION, BREAKPOINT, WATCHPOINT, MMIO_ACCESS, SYMBOL_ENTRY, CPU_ONLINE, STALL, BLOCKER
    pc: int
    symbol: str
    registers: Optional[Dict[str, str]] = None
    va: Optional[int] = None
    pa: Optional[int] = None
    value: Optional[int] = None
    instruction: Optional[str] = None
    metadata: Dict[str, str] = field(default_factory=dict)

class TraceStore:
    def __init__(self, session_id: str):
        self.session_id = session_id
        self.start_time = time.time()
        self.end_time: Optional[float] = None
        self.events: List[TraceEvent] = []

    def add_event(self, evt: TraceEvent):
        self.events.append(evt)

    def export(self, output_dir: str):
        os.makedirs(output_dir, exist_ok=True)
        self.end_time = time.time()

        # 1. Export trace.json
        trace_json_path = os.path.join(output_dir, "trace.json")
        with open(trace_json_path, "w", encoding="utf-8") as f:
            json.dump([asdict(e) for e in self.events], f, indent=2)

        # 2. Export metadata.json
        meta_json_path = os.path.join(output_dir, "metadata.json")
        meta = {
            "session_id": self.session_id,
            "start_time": self.start_time,
            "end_time": self.end_time,
            "duration": self.end_time - self.start_time,
            "total_events": len(self.events)
        }
        with open(meta_json_path, "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)

        # 3. Export timeline.md
        timeline_md_path = os.path.join(output_dir, "timeline.md")
        with open(timeline_md_path, "w", encoding="utf-8") as f:
            f.write(f"# Boot Trace Session: {self.session_id}\n\n")
            f.write(f"- Total Events: {len(self.events)}\n")
            f.write(f"- Duration: {meta['duration']:.2f}s\n\n")
            f.write("## Execution Event Log\n\n")
            f.write("| Timestamp (s) | CPU | Type | PC | Symbol |\n")
            f.write("|---|---|---|---|---|\n")
            for e in self.events[:50]:  # Cap first 50 events for clean preview
                rel_time = e.timestamp - self.start_time
                f.write(f"| {rel_time:.3f} | vCPU{e.cpu_id} | {e.event_type} | 0x{e.pc:016X} | {e.symbol} |\n")

class BootTraceRecorder:
    """
    Boot Trace Recorder for DevTools v1.1.
    Provides persistent multi-vCPU execution tracing, capturing events to structured session directories.
    """
    def __init__(self, session):
        self.session = session
        self.active_store: Optional[TraceStore] = None

    def start_session(self, session_id: Optional[str] = None) -> TraceStore:
        if not session_id:
            session_id = f"boot-{int(time.time())}"
        self.active_store = TraceStore(session_id)
        return self.active_store

    def record(self, cpu_id: int, event_type: str, pc: int, symbol: str, **kwargs) -> TraceEvent:
        if not self.active_store:
            self.start_session()
        evt = TraceEvent(
            timestamp=time.time(),
            cpu_id=cpu_id,
            event_type=event_type,
            pc=pc,
            symbol=symbol,
            registers=kwargs.get("registers"),
            va=kwargs.get("va"),
            pa=kwargs.get("pa"),
            value=kwargs.get("value"),
            instruction=kwargs.get("instruction"),
            metadata=kwargs.get("metadata", {})
        )
        self.active_store.add_event(evt)
        return evt

    def stop_session(self, output_dir: Optional[str] = None) -> str:
        if not self.active_store:
            return ""
        if not output_dir:
            output_dir = os.path.join("traces", self.active_store.session_id)
        self.active_store.export(output_dir)
        return output_dir
