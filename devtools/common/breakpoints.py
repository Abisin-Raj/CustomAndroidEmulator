import logging

class BreakpointManager:
    """
    Manages active target breakpoints (Z0/z0 software breakpoints, Z1/z1 hardware, etc.).
    Supports temporary/one-shot breakpoints and run-until logic.
    """
    def __init__(self, session):
        self.session = session
        self.active_breakpoints = {}  # addr -> (bp_type, size)
        self.logger = logging.getLogger("devtools.breakpoints")

    def add(self, addr: int, bp_type=0, size=4) -> bool:
        """Adds a breakpoint (0=software, 1=hardware)."""
        if addr in self.active_breakpoints:
            return True
        success = self.session.gdb.set_breakpoint(addr, bp_type, size)
        if success:
            self.active_breakpoints[addr] = (bp_type, size)
            self.logger.debug(f"Added breakpoint at 0x{addr:x} (type={bp_type})")
        else:
            self.logger.error(f"Failed to set breakpoint at 0x{addr:x}")
        return success

    def remove(self, addr: int) -> bool:
        """Removes an active breakpoint."""
        if addr not in self.active_breakpoints:
            return False
        bp_type, size = self.active_breakpoints[addr]
        success = self.session.gdb.remove_breakpoint(addr, bp_type, size)
        if success:
            del self.active_breakpoints[addr]
            self.logger.debug(f"Removed breakpoint at 0x{addr:x}")
        else:
            self.logger.error(f"Failed to remove breakpoint at 0x{addr:x}")
        return success

    def clear_all(self):
        """Clears all set breakpoints."""
        for addr in list(self.active_breakpoints.keys()):
            self.remove(addr)
        self.logger.info("Cleared all breakpoints.")
