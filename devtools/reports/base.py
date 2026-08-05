class ReportWriter:
    """
    Interface for formatting and writing analysis sessions to files.
    """
    def __init__(self, session):
        self.session = session

    def write(self, data: dict, output_path: str):
        """Writes the report file."""
        raise NotImplementedError()
