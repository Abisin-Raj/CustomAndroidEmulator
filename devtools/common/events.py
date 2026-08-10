class EventDispatcher:
    """
    Registry for execution callbacks (on_pc, on_symbol, on_exception, etc.).
    """
    def __init__(self, session):
        self.session = session
        self.pc_handlers = {}      # pc -> list of callables
        self.symbol_handlers = {}  # symbol_name -> list of callables
        self.step_handlers = []    # list of callables called on every instruction step

    def register_pc_handler(self, pc: int, handler):
        self.pc_handlers.setdefault(pc, []).append(handler)

    def register_symbol_handler(self, symbol_name: str, handler):
        self.symbol_handlers.setdefault(symbol_name, []).append(handler)

    def register_step_handler(self, handler):
        self.step_handlers.append(handler)

    def dispatch_step(self, pc: int, thread_id: str):
        """Dispatches step handlers and check for symbol/PC matches."""
        # 1. Execute general step handlers
        for handler in self.step_handlers:
            handler(pc, thread_id)

        # 2. Check PC match
        if pc in self.pc_handlers:
            for handler in self.pc_handlers[pc]:
                handler(pc, thread_id)

        # 3. Check Symbol match
        sym_info = self.session.symbols.resolve(pc)
        if sym_info and sym_info["offset"] == 0:
            sym_name = sym_info["name"]
            if sym_name in self.symbol_handlers:
                for handler in self.symbol_handlers[sym_name]:
                    handler(pc, thread_id)
class ExceptionEvent:
    pass

class MemoryAccessEvent:
    """
    Structured event payload captured when a memory watchpoint is hit.
    """
    def __init__(self, access_type: str, cpu_id: int, pc: int, symbol: str,
                 va: int, pa: str, value: int, registers: dict, call_site: str, timestamp: float):
        self.access_type = access_type
        self.cpu_id = cpu_id
        self.pc = pc
        self.symbol = symbol
        self.va = va
        self.pa = pa
        self.value = value
        self.registers = registers
        self.call_site = call_site
        self.timestamp = timestamp

    def format_event(self) -> str:
        out = []
        out.append(f"[{self.access_type}]")
        out.append(f"CPU:    vCPU{self.cpu_id}")
        out.append(f"PC:     0x{self.pc:016X}")
        out.append(f"Symbol: {self.symbol}")
        out.append(f"VA:     0x{self.va:016X}")
        out.append(f"PA:     {self.pa}")
        out.append(f"Value:  0x{self.value:X}")
        out.append(f"Caller: {self.call_site}")
        return "\n".join(out)

class MMIOAccessEvent:
    """
    Structured event payload captured when an MMIO read/write operation occurs inside a monitored physical range.
    """
    def __init__(self, access_type: str, cpu_id: int, pc: int, symbol: str,
                 va: int, pa: int, size: int, value: int, region_name: str,
                 instruction: str, call_site: str, timestamp: float):
        self.access_type = access_type
        self.cpu_id = cpu_id
        self.pc = pc
        self.symbol = symbol
        self.va = va
        self.pa = pa
        self.size = size
        self.value = value
        self.region_name = region_name
        self.instruction = instruction
        self.call_site = call_site
        self.timestamp = timestamp

    def format_event(self) -> str:
        out = []
        out.append(f"[{self.access_type}]")
        out.append(f"CPU:         vCPU{self.cpu_id}")
        out.append(f"PC:          0x{self.pc:016X}")
        out.append(f"Symbol:      {self.symbol}")
        out.append(f"VA:          0x{self.va:016X}")
        out.append(f"PA:          0x{self.pa:08X}")
        out.append(f"Size:        {self.size} byte(s)")
        out.append(f"Value:       0x{self.value:X}")
        out.append(f"Region:      {self.region_name}")
        out.append(f"Instruction: {self.instruction}")
        out.append(f"Caller:      {self.call_site}")
        return "\n".join(out)



