import json
import yaml
from typing import Dict, List, Optional, Any
from devtools.ai.schema import (
    ConfidenceLevel,
    EvidenceSource,
    Evidence,
    Constraint,
    RegisterContract,
    DeviceContract,
    CompatibilityProfile
)

class HardwareEvidenceDatabase:
    """
    Evidence-backed Hardware Evidence Database.
    Stores and manages hardware contracts, constraints, and auditability evidence for ROM compatibility profiles.
    """
    def __init__(self, rom_name: str = "PixelExperience_violet", kernel_version: str = "4.14.117"):
        self.profile = CompatibilityProfile(rom_name=rom_name, kernel_version=kernel_version)

    def add_device(self, device: DeviceContract) -> None:
        self.profile.devices[device.name] = device

    def get_device(self, name: str) -> Optional[DeviceContract]:
        return self.profile.devices.get(name)

    def export_yaml(self, file_path: str) -> None:
        data = self.profile.to_dict()
        with open(file_path, "w", encoding="utf-8") as f:
            yaml.dump(data, f, sort_keys=False, default_flow_style=False)

    def export_json(self, file_path: str) -> None:
        data = self.profile.to_dict()
        with open(file_path, "w", encoding="utf-8") as f:
            json.dump(data, f, indent=2)
