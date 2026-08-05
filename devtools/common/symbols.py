import os
import logging
import bisect

class SymbolService:
    """
    Manages symbol lookup and address-to-symbol resolution for target debugging.
    Loads System.map and performs binary search lookup.
    """
    def __init__(self, session):
        self.session = session
        self.logger = logging.getLogger("devtools.symbols")
        self.symbols = []  # list of tuples (address, type, name)
        self.address_keys = []  # address keys for binary search

    def load_map(self, map_filepath: str) -> bool:
        """Loads and parses a System.map file."""
        if not os.path.exists(map_filepath):
            self.logger.warning(f"System.map file not found at: {map_filepath}")
            return False

        self.logger.info(f"Loading System.map from {map_filepath}...")
        parsed_symbols = []
        try:
            with open(map_filepath, "r") as f:
                for line in f:
                    # Skip comment lines
                    if line.strip().startswith("#"):
                        continue
                    parts = line.strip().split()
                    if len(parts) >= 3:
                        try:
                            addr = int(parts[0], 16)
                            sym_type = parts[1]
                            name = parts[2]
                            parsed_symbols.append((addr, sym_type, name))
                        except ValueError:
                            pass
            
            # Sort symbols by address
            parsed_symbols.sort(key=lambda x: x[0])
            self.symbols = parsed_symbols
            self.address_keys = [s[0] for s in self.symbols]
            self.logger.info(f"Successfully loaded {len(self.symbols)} symbols.")
            return True
        except Exception as e:
            self.logger.error(f"Failed to load System.map: {e}")
            return False

    def resolve(self, addr: int) -> dict:
        """
        Resolves an address to its nearest preceding symbol and computes offset.
        Returns:
            dict containing: { "name": str, "address": int, "offset": int, "type": str }
        """
        if not self.symbols:
            return {"name": f"0x{addr:016x}", "address": addr, "offset": 0, "type": "U"}

        # Perform binary search (bisect_right returns index of first element > addr)
        idx = bisect.bisect_right(self.address_keys, addr)
        if idx == 0:
            # Address is before any known symbol
            return {"name": f"0x{addr:016x}", "address": addr, "offset": 0, "type": "U"}

        # Retrieve matching preceding symbol
        sym_addr, sym_type, sym_name = self.symbols[idx - 1]
        offset = addr - sym_addr
        
        # Guard against excessively large offsets that might cross functions
        if offset > 0x10000:
            return {"name": f"0x{addr:016x}", "address": addr, "offset": 0, "type": "U"}

        return {
            "name": sym_name,
            "address": sym_addr,
            "offset": offset,
            "type": sym_type
        }

    def get_symbol_address(self, name: str) -> int:
        """Finds the address of a symbol by name."""
        for addr, _, sym_name in self.symbols:
            if sym_name == name:
                return addr
        return None
