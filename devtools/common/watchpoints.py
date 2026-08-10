import time
import logging
from devtools.common.events import MemoryAccessEvent

class MemoryWatchpointTracer:
    """
    Traces memory read/write operations using GDB hardware watchpoints (Z2/Z3/Z4).
    Captures complete execution context upon hit (CPU, PC, Symbol, VA, PA, Value, Registers, Caller).
    """
    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.watchpoints")
        self.active_watchpoints = {}  # va -> { "size": int, "type": str, "callback": func }

    def add_watchpoint(self, va: int, size=8, access_type="WRITE", callback=None) -> bool:
        """
        Adds a memory watchpoint.
        access_type can be "WRITE" (Z2), "READ" (Z3), or "ACCESS" (Z4).
        """
        bp_type = 2  # default write watchpoint Z2
        if access_type.upper() == "READ":
            bp_type = 3
        elif access_type.upper() == "ACCESS":
            bp_type = 4

        success = self.session.gdb.set_breakpoint(va, bp_type=bp_type, size=size)
        if success:
            self.active_watchpoints[va] = {
                "size": size,
                "type": access_type.upper(),
                "callback": callback
            }
            self.logger.info(f"Added memory watchpoint at 0x{va:x} (type={access_type}, size={size})")
        else:
            self.logger.error(f"Failed to set memory watchpoint at 0x{va:x}")
        return success

    def remove_watchpoint(self, va: int) -> bool:
        """Removes an active watchpoint."""
        if va not in self.active_watchpoints:
            return False
        wp_info = self.active_watchpoints[va]
        access_type = wp_info["type"]
        bp_type = 2 if access_type == "WRITE" else (3 if access_type == "READ" else 4)
        
        success = self.session.gdb.remove_breakpoint(va, bp_type=bp_type, size=wp_info["size"])
        if success:
            del self.active_watchpoints[va]
            self.logger.info(f"Removed memory watchpoint at 0x{va:x}")
        return success

    def handle_hit(self, va: int, cpu_id=0, thread_id="p01.01") -> MemoryAccessEvent:
        """
        Constructs a structured MemoryAccessEvent upon watchpoint hit.
        """
        pc = self.session.gdb.read_register(self.session.snapshots.REG_PC, thread_id)
        lr = self.session.gdb.read_register(30, thread_id)  # LR
        
        sym_info = self.session.symbols.resolve(pc)
        symbol = f"{sym_info['name']}+0x{sym_info['offset']:x}" if sym_info else f"0x{pc:x}"
        
        lr_sym = self.session.symbols.resolve(lr)
        call_site = f"{lr_sym['name']}+0x{lr_sym['offset']:x}" if lr_sym else f"0x{lr:x}"

        # Resolve Physical Address via HMP gva2gpa
        pa_out = self.session.gdb.run_hmp_cmd(f"gva2gpa 0x{va:x}")
        pa = pa_out.strip() if pa_out else "Unknown"

        # Read current memory value at target VA
        size = self.active_watchpoints.get(va, {}).get("size", 8)
        if size == 8:
            val = self.session.memory.read_u64(va, thread_id)
        elif size == 4:
            val = self.session.memory.read_u32(va, thread_id)
        else:
            val = self.session.memory.read_u8(va, thread_id)

        # Capture register snapshot dictionary
        regs = {}
        for r in range(31):
            regs[f"X{r}"] = self.session.gdb.read_register(r, thread_id)

        wp_info = self.active_watchpoints.get(va, {})
        access_type = wp_info.get("type", "WRITE")

        event = MemoryAccessEvent(
            access_type=access_type,
            cpu_id=cpu_id,
            pc=pc,
            symbol=symbol,
            va=va,
            pa=pa,
            value=val,
            registers=regs,
            call_site=call_site,
            timestamp=time.time()
        )

        # Invoke callback if registered
        cb = wp_info.get("callback")
        if cb:
            cb(event)

        return event
