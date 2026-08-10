import unittest
from devtools.dt.parser import DTSParser, DTNode, HardwareInventory
from devtools.dt.comparator import DeviceTreeComparator

class TestDeviceTreeAnalyzer(unittest.TestCase):
    def test_dts_parser_node_extraction(self):
        sample_dts = """
        qcom,gcc@100000 {
            compatible = "qcom,gcc-sm6150", "syscon";
            reg = <0x100000 0x1f0000>;
            vdd_cx-supply = <0x1d>;
            phandle = <0x31>;
        };
        """
        inventory = DTSParser.parse_text(sample_dts)
        self.assertIn("qcom,gcc-sm6150", inventory.compatible_map)
        node = inventory.find_by_compatible("qcom,gcc-sm6150")
        self.assertIsNotNone(node)
        self.assertEqual(node.name, "qcom,gcc@100000")
        self.assertEqual(node.reg, [(0x100000, 0x1F0000)])
        self.assertIn("vdd_cx-supply", node.supplies)

    def test_dt_comparator_diff(self):
        target_dts = """
        qcom,gcc@100000 {
            compatible = "qcom,gcc-sm6150";
            reg = <0x100000 0x1f0000>;
        };
        """
        qemu_dts = """
        timer {
            compatible = "arm,armv8-timer";
        };
        """
        target_inv = DTSParser.parse_text(target_dts)
        qemu_inv = DTSParser.parse_text(qemu_dts)

        diff = DeviceTreeComparator.compare(target_inv, qemu_inv, target_name="Target", qemu_name="QEMU")
        self.assertEqual(len(diff.missing_nodes), 1)
        self.assertEqual(diff.missing_nodes[0].name, "qcom,gcc@100000")
        self.assertEqual(diff.missing_compatibles, ["qcom,gcc-sm6150"])

        formatted = diff.format_report()
        self.assertIn("DRIVER PROBE IMPOSSIBLE", formatted)
        self.assertIn("qcom,gcc-sm6150", formatted)
        self.assertIn("0x00100000", formatted)

if __name__ == "__main__":
    unittest.main()
