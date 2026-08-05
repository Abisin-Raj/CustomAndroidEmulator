import json
from devtools.reports.base import ReportWriter

class JsonReport(ReportWriter):
    """
    Generates structured, machine-readable JSON reports.
    """
    def write(self, data: dict, output_path: str):
        with open(output_path, "w") as f:
            json.dump(data, f, indent=2)
