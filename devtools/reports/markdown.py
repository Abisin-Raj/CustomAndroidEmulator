from devtools.reports.base import ReportWriter

class MarkdownReport(ReportWriter):
    """
    Generates beautiful, structured Markdown reports for validation tasks.
    """
    def write(self, data: dict, output_path: str):
        lines = []
        lines.append(f"# Analysis Report: {data.get('title', 'DevTools Report')}")
        lines.append(f"**Execution Status**: {data.get('status', 'N/A')}\n")
        
        # Summary Section
        lines.append("## Executive Summary")
        lines.append(f"{data.get('summary', 'No summary provided.')}\n")

        # Context details
        lines.append("## Environment details")
        for k, v in data.get("env", {}).items():
            lines.append(f"- **{k}**: {v}")
        lines.append("")

        # Step milestones / Log
        if "timeline" in data:
            lines.append("## Event Timeline")
            for item in data["timeline"]:
                lines.append(f"- **{item['timestamp']}** (CPU {item['cpu']}): {item['event']}")
            lines.append("")

        # Register Snapshot
        if "registers" in data:
            lines.append("## Register Dump")
            lines.append("```text")
            lines.append(data["registers"])
            lines.append("```\n")

        # Disassembly
        if "disassembly" in data:
            lines.append("## Disassembly Context")
            lines.append("```text")
            for line in data["disassembly"]:
                lines.append(line)
            lines.append("```\n")

        # Write to file
        with open(output_path, "w") as f:
            f.write("\n".join(lines))
