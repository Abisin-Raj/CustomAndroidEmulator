import logging

class TraceSession:
    """
    TraceSession is the central controller encapsulating all debugger services.
    All modules, plugins, and CLI commands access GDB state, symbols, memory,
    and breakpoints through this object.
    """
    def __init__(self, config=None):
        self.config = config or {}
        self.logger = logging.getLogger("devtools.session")
        
        # Services - populated in subsequent milestones
        from devtools.common.gdb_client import GdbClient
        from devtools.common.symbols import SymbolService
        from devtools.common.snapshots import RegisterSnapshotEngine
        from devtools.common.breakpoints import BreakpointManager
        from devtools.common.events import EventDispatcher
        from devtools.common.memory import MemoryService
        from devtools.common.disasm import DisassemblerService
        from devtools.trace.engine import TraceEngine
        from devtools.common.watchpoints import MemoryWatchpointTracer
        self.gdb = None
        self.symbols = SymbolService(self)
        self.snapshots = RegisterSnapshotEngine(self)
        self.breakpoints = BreakpointManager(self)
        self.events = EventDispatcher(self)
        self.memory = MemoryService(self)
        self.disasm = DisassemblerService(self)
        self.trace = TraceEngine(self)
        self.watchpoints = MemoryWatchpointTracer(self)
        self.timeline = None
        self.report = None
        self.logger = logging.getLogger("devtools.session")








    def connect(self, host="127.0.0.1", port=1234):
        """Establish GDB RSP connection."""
        from devtools.common.gdb_client import GdbClient
        self.gdb = GdbClient(host, port)
        self.gdb.connect()

    def disconnect(self):
        """Disconnect and clean up GDB session."""
        if self.gdb:
            self.gdb.disconnect()
            self.gdb = None

