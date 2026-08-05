class LoopDetector:
    """
    Tracks instruction execution patterns to detect busy-waits, spinlocks,
    infinite loops, and retry patterns.
    """
    def __init__(self, history_size=256):
        self.history_size = history_size
        self.pc_history = []
        self.detected_loop = None

    def record_step(self, pc: int) -> dict:
        """Records a step and evaluates if it resides within a recurring loop pattern."""
        self.pc_history.append(pc)
        if len(self.pc_history) > self.history_size:
            self.pc_history.pop(0)

        # Detect loop patterns by checking repeating sub-sequences of PCs
        history_len = len(self.pc_history)
        for pattern_len in range(1, history_len // 2 + 1):
            # Extract candidate pattern from the end of the history
            pattern = self.pc_history[-pattern_len:]
            # Check the preceding window of the same length
            preceding = self.pc_history[-2*pattern_len:-pattern_len]
            if pattern == preceding:
                # Loop pattern match confirmed
                self.detected_loop = {
                    "start_pc": pattern[0],
                    "end_pc": pattern[-1],
                    "length": pattern_len,
                    "pattern": pattern
                }
                return self.detected_loop

        self.detected_loop = None
        return None
