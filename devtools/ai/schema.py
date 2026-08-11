from enum import Enum
from dataclasses import dataclass, field, asdict
from typing import Dict, List, Optional, Any

class ConfidenceLevel(str, Enum):
    EXACT = "EXACT"          # Directly extracted from DTB or kernel source
    DERIVED = "DERIVED"      # Inferred from kernel instructions / branches
    OBSERVED = "OBSERVED"    # Confirmed through QEMU runtime tracing
    INFERRED = "INFERRED"    # Strong conclusion from multiple pieces of evidence
    UNKNOWN = "UNKNOWN"      # Insufficient evidence

class EvidenceSource(str, Enum):
    DTB = "DTB"
    DISASSEMBLY = "DISASSEMBLY"
    RUNTIME_TRACE = "RUNTIME_TRACE"
    KERNEL_CONFIG = "KERNEL_CONFIG"
    HEURISTIC = "HEURISTIC"

@dataclass
class Evidence:
    source: EvidenceSource
    confidence: ConfidenceLevel
    description: str
    reference: Optional[str] = None

    def to_dict(self) -> Dict[str, Any]:
        return {
            "source": self.source.value,
            "confidence": self.confidence.value,
            "description": self.description,
            "reference": self.reference
        }

@dataclass
class Constraint:
    expression: str
    mask: Optional[int] = None
    expected_value: Optional[int] = None
    description: str = ""

    def to_dict(self) -> Dict[str, Any]:
        res = {"expression": self.expression, "description": self.description}
        if self.mask is not None:
            res["mask"] = f"0x{self.mask:X}"
        if self.expected_value is not None:
            res["expected_value"] = f"0x{self.expected_value:X}"
        return res

@dataclass
class RegisterContract:
    offset: int
    name: str
    access_type: str = "READ_WRITE"  # READ, WRITE, READ_WRITE
    default_value: int = 0
    constraints: List[Constraint] = field(default_factory=list)
    evidence: List[Evidence] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "offset": f"0x{self.offset:04X}",
            "name": self.name,
            "access_type": self.access_type,
            "default_value": f"0x{self.default_value:08X}",
            "constraints": [c.to_dict() for c in self.constraints],
            "evidence": [e.to_dict() for e in self.evidence]
        }

@dataclass
class DeviceContract:
    name: str
    compatible: str
    mmio_base: Optional[int] = None
    mmio_size: Optional[int] = None
    irq: Optional[int] = None
    registers: Dict[str, RegisterContract] = field(default_factory=dict)
    properties: Dict[str, Any] = field(default_factory=dict)
    evidence: List[Evidence] = field(default_factory=list)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "name": self.name,
            "compatible": self.compatible,
            "mmio_base": f"0x{self.mmio_base:08X}" if self.mmio_base is not None else None,
            "mmio_size": f"0x{self.mmio_size:08X}" if self.mmio_size is not None else None,
            "irq": self.irq,
            "registers": {k: v.to_dict() for k, v in self.registers.items()},
            "properties": self.properties,
            "evidence": [e.to_dict() for e in self.evidence]
        }

@dataclass
class CompatibilityProfile:
    rom_name: str
    kernel_version: str
    arch: str = "aarch64"
    devices: Dict[str, DeviceContract] = field(default_factory=dict)

    def to_dict(self) -> Dict[str, Any]:
        return {
            "rom": {
                "name": self.rom_name,
                "kernel": self.kernel_version,
                "arch": self.arch
            },
            "hardware": {k: v.to_dict() for k, v in self.devices.items()}
        }
