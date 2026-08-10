from dataclasses import dataclass, field
from typing import List, Dict, Tuple
from devtools.dt.parser import DTNode, HardwareInventory

@dataclass
class DeviceTreeDiff:
    target_name: str
    qemu_name: str
    missing_nodes: List[DTNode] = field(default_factory=list)
    missing_compatibles: List[str] = field(default_factory=list)
    missing_mmio_regions: List[Tuple[str, int, int]] = field(default_factory=list)  # (name, base, size)
    dependencies: Dict[str, List[str]] = field(default_factory=dict)

    def format_report(self) -> str:
        out = []
        out.append("# DEVICE TREE COMPATIBILITY & HARDWARE CONTRACT REPORT")
        out.append(f"**Target DTS**: `{self.target_name}` | **QEMU DTB**: `{self.qemu_name}`\n")
        
        if not self.missing_nodes:
            out.append("### Status: ALL TARGET HARDWARE NODES PRESENT")
            return "\n".join(out)

        out.append("### Status: DRIVER PROBE IMPOSSIBLE (Missing Vendor Hardware Contract)")
        out.append("\n## Missing Target Hardware Devices")
        
        for node in self.missing_nodes:
            out.append(f"\n### Device Node: `{node.name}`")
            out.append(f"- **Compatible String**: `{', '.join(node.compatible)}`")
            if node.reg:
                for base, size in node.reg:
                    out.append(f"- **MMIO Region**: Base `0x{base:08X}` | Size `0x{size:08X}` bytes")
            else:
                out.append("- **MMIO Region**: NOT SPECIFIED")

            if node.supplies:
                out.append(f"- **Power Supplies**: `{', '.join(node.supplies)}`")

            # QEMU status
            out.append("- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`")

        out.append("\n## Dependency Graph")
        for node_name, deps in self.dependencies.items():
            out.append(f"```text\n{node_name}\n" + "\n".join([f"    +-- {d}" for d in deps]) + "\n```")

        return "\n".join(out)


class DeviceTreeComparator:
    """
    Compares target hardware inventory against QEMU generated inventory.
    Identifies missing nodes, missing compatibles, missing MMIO ranges, and supply dependencies.
    """
    @staticmethod
    def compare(target_inv: HardwareInventory, qemu_inv: HardwareInventory,
                target_name="Target DT", qemu_name="QEMU DT") -> DeviceTreeDiff:
        diff = DeviceTreeDiff(target_name=target_name, qemu_name=qemu_name)

        for comp, node in target_inv.compatible_map.items():
            if comp not in qemu_inv.compatible_map:
                if node not in diff.missing_nodes:
                    diff.missing_nodes.append(node)
                    diff.missing_compatibles.append(comp)

                    if node.reg:
                        for base, size in node.reg:
                            diff.missing_mmio_regions.append((node.name, base, size))

                    deps = []
                    if node.supplies:
                        deps.extend(node.supplies)
                    if node.clocks:
                        deps.extend([f"clock: {c}" for c in node.clocks])
                    if node.resets:
                        deps.extend([f"reset: {r}" for r in node.resets])
                    
                    diff.dependencies[node.name] = deps

        return diff
