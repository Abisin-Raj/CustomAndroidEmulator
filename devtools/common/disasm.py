import logging
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_ARM

class DisassemblerService:
    """
    Capstone-based disassembler service to dynamically parse instructions from memory.
    """
    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.disasm")
        self.md = Cs(CS_ARCH_ARM64, CS_MODE_ARM)

    def disassemble(self, addr: int, size: int, thread="p01.01") -> list:
        """
        Reads memory and disassembles it.
        Returns:
            list of dicts containing: { "address": int, "mnemonic": str, "op_str": str, "bytes": bytes }
        """
        data = self.session.memory.read_bytes(addr, size, thread)
        if not data:
            return []

        insns = []
        for insn in self.md.disasm(data, addr):
            insns.append({
                "address": insn.address,
                "mnemonic": insn.mnemonic,
                "op_str": insn.op_str,
                "bytes": insn.bytes
            })
        return insns

    def format_instruction(self, insn: dict) -> str:
        """Formats a single instruction dict into a standard disassembly line."""
        addr = insn["address"]
        mnemonic = insn["mnemonic"]
        op_str = insn["op_str"]
        return f"0x{addr:016x}: {mnemonic:<10} {op_str}"
