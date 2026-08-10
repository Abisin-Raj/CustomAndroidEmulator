import re
from dataclasses import dataclass, field
from typing import List, Dict, Tuple, Optional

@dataclass
class DTNode:
    name: str
    compatible: List[str] = field(default_factory=list)
    reg: List[Tuple[int, int]] = field(default_factory=list)  # (base, size)
    interrupts: List[int] = field(default_factory=list)
    clocks: List[str] = field(default_factory=list)
    resets: List[str] = field(default_factory=list)
    supplies: List[str] = field(default_factory=list)
    phandle: Optional[int] = None
    properties: Dict[str, str] = field(default_factory=dict)

class HardwareInventory:
    def __init__(self):
        self.nodes: List[DTNode] = []
        self.compatible_map: Dict[str, DTNode] = {}

    def add_node(self, node: DTNode):
        self.nodes.append(node)
        for comp in node.compatible:
            self.compatible_map[comp] = node

    def find_by_compatible(self, comp: str) -> Optional[DTNode]:
        return self.compatible_map.get(comp)

class DTSParser:
    """
    DTS Text Parser for Device Tree Source (.dts) files.
    Extracts structured DTNode hierarchies, compatibles, MMIO reg ranges, and dependencies.
    """
    @staticmethod
    def parse_file(file_path: str) -> HardwareInventory:
        with open(file_path, "r", encoding="utf-8", errors="ignore") as f:
            content = f.read()
        return DTSParser.parse_text(content)

    @staticmethod
    def parse_text(text: str) -> HardwareInventory:
        inventory = HardwareInventory()

        # Regex pattern matching DT nodes: node_name { ... };
        node_blocks = re.findall(r'([a-zA-Z0-9_,\-@]+)\s*\{([^}]+)\};', text, re.MULTILINE)

        for name, block in node_blocks:
            compatible_match = re.search(r'compatible\s*=\s*"([^"]+)"(?:\s*,\s*"([^"]+)")*;', block)
            compatibles = []
            if compatible_match:
                compatibles = [c for c in compatible_match.groups() if c]

            # Extract reg = <base size>;
            reg_match = re.search(r'reg\s*=\s*<([^>]+)>;', block)
            regs = []
            if reg_match:
                raw_nums = reg_match.group(1).strip().split()
                nums = []
                for n in raw_nums:
                    try:
                        nums.append(int(n, 16) if n.startswith("0x") else int(n))
                    except ValueError:
                        pass
                if len(nums) >= 2:
                    regs.append((nums[0], nums[1]))

            # Extract supplies
            supplies = re.findall(r'([a-zA-Z0-9_\-]+-supply)\s*=', block)

            # Extract phandle
            phandle_match = re.search(r'phandle\s*=\s*<([^>]+)>;', block)
            phandle = None
            if phandle_match:
                try:
                    ph_str = phandle_match.group(1).strip()
                    phandle = int(ph_str, 16) if ph_str.startswith("0x") else int(ph_str)
                except ValueError:
                    pass

            node = DTNode(
                name=name.strip(),
                compatible=compatibles,
                reg=regs,
                supplies=supplies,
                phandle=phandle
            )
            inventory.add_node(node)

        return inventory
