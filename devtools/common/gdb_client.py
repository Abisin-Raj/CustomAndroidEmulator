import socket
import logging
import struct

class GdbClient:
    """
    RSP (Remote Serial Protocol) client for GDB stub communication.
    Handles low-level packet construction, checksum validation, and socket transmission.
    """
    def __init__(self, host="127.0.0.1", port=1234):
        self.host = host
        self.port = port
        self.sock = None
        self.logger = logging.getLogger("devtools.gdb")

    def connect(self):
        """Establishes the socket connection to the GDB stub."""
        self.logger.info(f"Connecting to GDB stub at {self.host}:{self.port}...")
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5.0)
        self.sock.connect((self.host, self.port))
        # Send initial Ack and drain initial stop response if present
        self.sock.sendall(b"+")
        try:
            self.sock.recv(4096)
        except Exception:
            pass
        self.logger.info("Connected to GDB stub successfully.")


    def disconnect(self):
        """Closes the GDB socket."""
        if self.sock:
            self.sock.close()
            self.sock = None
            self.logger.info("Disconnected from GDB stub.")

    def _calculate_checksum(self, payload: str) -> str:
        """Computes the standard GDB checksum (modulo 256 sum of bytes)."""
        return f"{sum(payload.encode('utf-8')) % 256:02x}"

    def send_packet(self, payload: str) -> str:
        """Sends an RSP payload to GDB and waits for the acknowledged response."""
        if not self.sock:
            raise RuntimeError("GdbClient is not connected.")

        checksum = self._calculate_checksum(payload)
        packet = f"${payload}#{checksum}"
        self.sock.sendall(packet.encode("utf-8"))

        # Wait for GDB ACK (+) and then read the packet
        response = ""
        while True:
            try:
                chunk = self.sock.recv(4096).decode(errors="ignore")
                if not chunk:
                    break
                response += chunk
                if "$" in response and "#" in response:
                    break
                if response == "+":
                    continue
            except Exception as e:
                self.logger.error(f"Error reading GDB response: {e}")
                break


        # Acknowledge receipt
        self.sock.sendall(b"+")

        # Strip ACK (+) and parse the packet content out
        if "$" in response:
            payload_start = response.find("$") + 1
            payload_end = response.rfind("#")
            return response[payload_start:payload_end]
        return ""

    def select_thread(self, thread_id: str, mode="g") -> bool:
        """
        Sends Hg or Hc to select active thread.
        mode 'g' is for general operations (registers/mem), 'c' is for control (step/continue).
        """
        resp = self.send_packet(f"H{mode}{thread_id}")
        return resp == "OK"

    def read_register(self, reg_num: int, thread_id="p01.01") -> int:
        """Reads a register value from GDB."""
        self.select_thread(thread_id, "g")
        resp = self.send_packet(f"p{reg_num:x}")
        if resp and not resp.startswith("E"):
            try:
                # Hex values are returned in little-endian order
                val_bytes = bytes.fromhex(resp[:16])
                if len(val_bytes) == 8:
                    return struct.unpack("<Q", val_bytes)[0]
            except Exception:
                pass
        return 0


    def write_register(self, reg_num: int, val: int, thread_id="p01.01") -> bool:
        """Writes a register value to GDB."""
        self.select_thread(thread_id, "g")
        val_hex = struct.pack("<Q", val & 0xFFFFFFFFFFFFFFFF).hex()
        resp = self.send_packet(f"P{reg_num:x}={val_hex}")
        return resp == "OK"

    def read_memory(self, addr: int, size: int, thread_id="p01.01") -> bytes:
        """Reads bytes from GDB memory."""
        self.select_thread(thread_id, "g")
        resp = self.send_packet(f"m{addr:x},{size}")
        if resp and not resp.startswith("E"):
            try:
                return bytes.fromhex(resp[:size*2])
            except ValueError:
                pass
        return b""

    def write_memory(self, addr: int, data: bytes, thread_id="p01.01") -> bool:
        """Writes bytes to GDB memory."""
        self.select_thread(thread_id, "g")
        data_hex = data.hex()
        resp = self.send_packet(f"M{addr:x},{len(data):x}:{data_hex}")
        return resp == "OK"

    def set_breakpoint(self, addr: int, bp_type=0, size=4) -> bool:
        """Sets a breakpoint of type (0 = software, 1 = hardware)."""
        resp = self.send_packet(f"Z{bp_type},{addr:x},{size}")
        return resp == "OK"

    def remove_breakpoint(self, addr: int, bp_type=0, size=4) -> bool:
        """Removes a breakpoint of type (0 = software, 1 = hardware)."""
        resp = self.send_packet(f"z{bp_type},{addr:x},{size}")
        return resp == "OK"

    def step(self, thread_id="p01.01") -> bool:
        """Steps the selected thread once."""
        self.select_thread(thread_id, "c")
        resp = self.send_packet(f"vCont;s:{thread_id}")
        return resp != ""

    def resume(self, thread_id="p01.01") -> bool:
        """Resumes the selected thread."""
        self.select_thread(thread_id, "c")
        resp = self.send_packet(f"vCont;c:{thread_id}")
        return resp != ""

    def run_hmp_cmd(self, hmp_cmd: str) -> str:
        """Executes a QEMU Monitor (HMP) command using GDB's qRcmd."""
        hex_cmd = hmp_cmd.encode("utf-8").hex()
        resp = self.send_packet(f"qRcmd,{hex_cmd}")
        out = ""
        # GDB streams HMP output chunks prefixed with "O" hex payload
        for chunk in resp.split("O"):
            if chunk:
                # Strip checksum if present at end
                clean = chunk.split("#")[0]
                try:
                    out += bytes.fromhex(clean).decode(errors="ignore")
                except Exception:
                    pass
        return out
