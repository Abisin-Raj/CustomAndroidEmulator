import unittest
import os
import yaml
from devtools.ai.schema import (
    ConfidenceLevel,
    EvidenceSource,
    Evidence,
    Constraint,
    RegisterContract,
    DeviceContract,
    CompatibilityProfile
)
from devtools.ai.database import HardwareEvidenceDatabase
from devtools.ai.profile import ProfileEngine

class TestAIEngine(unittest.TestCase):
    def test_schema_serialization(self):
        ev = Evidence(source=EvidenceSource.DTB, confidence=ConfidenceLevel.EXACT, description="Test DTB node", reference="kernel.dts:10")
        c = Constraint(expression="bit_31 == 0", mask=0x80000000, expected_value=0x0, description="Bit 31 clear")
        reg = RegisterContract(offset=0x180, name="TEST_REG", default_value=0x0, constraints=[c], evidence=[ev])
        dev = DeviceContract(name="test_dev", compatible="qcom,test-device", mmio_base=0x100000, mmio_size=0x1000, registers={"0x0180": reg}, evidence=[ev])
        prof = CompatibilityProfile(rom_name="TestROM", kernel_version="4.14", devices={"test_dev": dev})

        data = prof.to_dict()
        self.assertEqual(data["rom"]["name"], "TestROM")
        self.assertEqual(data["hardware"]["test_dev"]["compatible"], "qcom,test-device")
        self.assertEqual(data["hardware"]["test_dev"]["registers"]["0x0180"]["constraints"][0]["expression"], "bit_31 == 0")

    def test_profile_generation(self):
        engine = ProfileEngine()
        profile = engine.generate_full_sm6150_profile()
        self.assertEqual(profile.rom_name, "PixelExperience_violet")
        self.assertIn("gcc", profile.devices)
        self.assertIn("timer", profile.devices)
        self.assertIn("psci", profile.devices)
        self.assertIn("rpmh_rsc", profile.devices)
        self.assertIn("rpmh_clock", profile.devices)

        # Export YAML profile test
        db = HardwareEvidenceDatabase()
        db.profile = profile
        out_yaml = "configs/pixel_experience_profile.yaml"
        db.export_yaml(out_yaml)
        self.assertTrue(os.path.exists(out_yaml))

        with open(out_yaml, "r", encoding="utf-8") as f:
            yaml_data = yaml.safe_load(f)
        self.assertEqual(yaml_data["rom"]["name"], "PixelExperience_violet")
        self.assertEqual(yaml_data["hardware"]["gcc"]["compatible"], "qcom,gcc-sm6150")
        self.assertEqual(yaml_data["hardware"]["rpmh_rsc"]["compatible"], "qcom,tcs-drv")

if __name__ == "__main__":
    unittest.main()
