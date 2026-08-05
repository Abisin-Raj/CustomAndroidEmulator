class MemoryService:
    """
    Exposes a rich, structured memory reading API via GDB RSP.
    """
    def __init__(self, session):
        self.session = session

    def read_bytes(self, addr, size, thread="p01.01") -> bytes:
        return self.session.gdb.read_memory(addr, size, thread)


    def read_u8(self, addr, thread="p01.01") -> int:
        data = self.read_bytes(addr, 1, thread)
        return data[0] if data else 0

    def read_u16(self, addr, thread="p01.01") -> int:
        data = self.read_bytes(addr, 2, thread)
        if len(data) == 2:
            import struct
            return struct.unpack("<H", data)[0]
        return 0

    def read_u32(self, addr, thread="p01.01") -> int:
        data = self.read_bytes(addr, 4, thread)
        if len(data) == 4:
            import struct
            return struct.unpack("<I", data)[0]
        return 0

    def read_u64(self, addr, thread="p01.01") -> int:
        data = self.read_bytes(addr, 8, thread)
        if len(data) == 8:
            import struct
            return struct.unpack("<Q", data)[0]
        return 0

    def read_string(self, addr, max_len=128, thread="p01.01") -> str:
        """Reads a null-terminated C-string from target memory."""
        data = self.read_bytes(addr, max_len, thread)
        if not data:
            return ""
        null_idx = data.find(b"\x00")
        if null_idx != -1:
            data = data[:null_idx]
        return data.decode(errors="replace")

    def read_struct(self, addr, struct_format, thread="p01.01") -> tuple:
        """Reads and unpacks a struct using python's struct.unpack."""
        import struct
        size = struct.calcsize(struct_format)
        data = self.read_bytes(addr, size, thread)
        if len(data) == size:
            return struct.unpack(struct_format, data)
        raise ValueError(f"Could not read complete struct of size {size} at 0x{addr:x}")
