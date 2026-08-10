import unittest
from unittest.mock import MagicMock
from devtools.common.events import MMIOAccessEvent
from devtools.common.mmio_tracer import MMIORegion, MMIOTracer

class TestMMIOTracer(unittest.TestCase):
    def test_mmio_region_containment(self):
        region = MMIORegion(name="qcom-gcc", base=0x00100000, size=0x001F0000)
        self.assertEqual(region.end, 0x002F0000)
        self.assertTrue(region.contains_pa(0x00100000))
        self.assertTrue(region.contains_pa(0x002EFFFF))
        self.assertFalse(region.contains_pa(0x002F0000))
        self.assertFalse(region.contains_pa(0x000FFFFF))

    def test_mmio_access_event_formatting(self):
        event = MMIOAccessEvent(
            access_type="MMIO READ",
            cpu_id=0,
            pc=0xFFFFFF80080DD3B4,
            symbol="qcom_cc_probe+0x84",
            va=0xFFFFFF800A931BE0,
            pa=0x00100000,
            size=4,
            value=0x1,
            region_name="qcom-gcc",
            instruction="ldurb w8, [x26, #-8]",
            call_site="do_initcalls+0x10",
            timestamp=1000.0
        )
        formatted = event.format_event()
        self.assertIn("[MMIO READ]", formatted)
        self.assertIn("vCPU0", formatted)
        self.assertIn("0xFFFFFF80080DD3B4", formatted)
        self.assertIn("qcom_cc_probe+0x84", formatted)
        self.assertIn("0x00100000", formatted)
        self.assertIn("qcom-gcc", formatted)
        self.assertIn("ldurb w8, [x26, #-8]", formatted)

    def test_mmio_tracer_region_management(self):
        mock_session = MagicMock()
        tracer = MMIOTracer(mock_session)
        
        region = tracer.add_region("qcom-gcc", base=0x00100000, size=0x001F0000)
        self.assertIn("qcom-gcc", tracer.regions)
        self.assertEqual(tracer.find_region(0x00150000), region)
        self.assertIsNone(tracer.find_region(0x40000000))

        rem_success = tracer.remove_region("qcom-gcc")
        self.assertTrue(rem_success)
        self.assertNotIn("qcom-gcc", tracer.regions)

if __name__ == "__main__":
    unittest.main()
