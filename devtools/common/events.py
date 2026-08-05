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

