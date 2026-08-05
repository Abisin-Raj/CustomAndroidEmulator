class Plugin:
    """
    Base class for all DevTools validation and analysis plugins.
    Plugins are supplied with the active TraceSession.
    """
    def __init__(self, session):
        self.session = session

    def run(self, args=None) -> int:
        """
        Executes the plugin logic.
        Returns:
            0 on success, non-zero exit code on failure.
        """
        raise NotImplementedError("Plugins must override run()")
