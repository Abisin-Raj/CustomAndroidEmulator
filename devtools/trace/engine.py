import logging

class TraceEngine:
    """
    Generic execution tracing engine. Runs stepping loops, evaluates
    breakpoint conditions, and dispatches events.
    """
    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.trace")
        self._stop_requested = False

    def request_stop(self):
        """Instructs the current trace execution loop to exit."""
        self._stop_requested = True

    def run_steps(self, max_steps: int, thread_id="p01.01") -> int:
        """
        Executes instructions step-by-step up to max_steps.
        Dispatches Events to the EventDispatcher on every step.
        Returns:
            The number of steps executed.
        """
        self._stop_requested = False
        steps_executed = 0
        
        self.logger.info(f"Starting trace engine stepping for thread {thread_id} (max_steps={max_steps})...")
        while not self._stop_requested and steps_executed < max_steps:
            # 1. Perform single instruction step
            success = self.session.gdb.step(thread_id)
            if not success:
                self.logger.error("GDB step command failed.")
                break
                
            steps_executed += 1
            
            # 2. Get current instruction pointer
            pc = self.session.gdb.read_register(self.session.snapshots.REG_PC, thread_id)
            
            # 3. Dispatch execution events
            self.session.events.dispatch_step(pc, thread_id)

        self.logger.info(f"Stepping loop terminated. Executed {steps_executed} steps.")
        return steps_executed

    def continue_until_pc(self, target_pc: int, timeout_sec=5.0, thread_id="p01.01") -> bool:
        """
        Runs the CPU using breakpoints until it reaches target_pc.
        """
        self.session.breakpoints.add(target_pc)
        self.logger.info(f"Continuing target execution until PC reaches 0x{target_pc:x}...")
        
        import time
        # Resume target execution
        self.session.gdb.resume(thread_id)
        time.sleep(timeout_sec)
        
        # Check current PC
        current_pc = self.session.gdb.read_register(self.session.snapshots.REG_PC, thread_id)
        self.session.breakpoints.remove(target_pc)
        
        if current_pc == target_pc:
            self.logger.info(f"Target successfully reached target PC 0x{target_pc:x}.")
            return True
        else:
            self.logger.warning(f"Target did not reach target PC. Stopped at 0x{current_pc:x}.")
            return False
