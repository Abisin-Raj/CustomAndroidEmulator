import unittest
from unittest.mock import MagicMock
from devtools.symbols.explorer import SymbolExplorer, CrossReference

class TestSymbolExplorer(unittest.TestCase):
    def test_symbol_lookup(self):
        mock_session = MagicMock()
        mock_session.symbols.resolve.return_value = {"name": "do_initcalls", "address": 0xFFFFFF80080DC200, "offset": 0x14}
        mock_session.symbols.lookup.return_value = 0xFFFFFF80080DD330

        explorer = SymbolExplorer(mock_session)
        
        res1 = explorer.lookup("0xFFFFFF80080DC214")
        self.assertEqual(res1["name"], "do_initcalls")

        res2 = explorer.lookup("gcc_sm6150_probe")
        self.assertEqual(res2["address"], 0xFFFFFF80080DD330)

    def test_find_xrefs_scanning(self):
        mock_session = MagicMock()
        explorer = SymbolExplorer(mock_session)

        sample_insns = [
            {"address": 0xFFFFFF80080DC214, "mnemonic": "ldr", "op_str": "x8, [x21, #0xbe0]"},
            {"address": 0xFFFFFF80080DC218, "mnemonic": "sub", "op_str": "x9, x8, x22"},
            {"address": 0xFFFFFF80080DC230, "mnemonic": "blr", "op_str": "x8"},
        ]

        xrefs = explorer.find_xrefs("0xbe0", sample_insns)
        self.assertEqual(len(xrefs), 1)
        self.assertEqual(xrefs[0].source_pc, 0xFFFFFF80080DC214)
        self.assertEqual(xrefs[0].ref_type, "READ")
        self.assertIn("#0xbe0", xrefs[0].op_str)

if __name__ == "__main__":
    unittest.main()
