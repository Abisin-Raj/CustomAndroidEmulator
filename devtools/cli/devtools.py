import argparse
import sys
import os
import yaml
import logging
import importlib.util

# Set up logging format
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s"
)
logger = logging.getLogger("devtools.cli")

def load_config(config_path: str) -> dict:
    """Loads target config YAML file."""
    if not os.path.exists(config_path):
        logger.error(f"Config file not found: {config_path}")
        sys.exit(1)
    with open(config_path, "r") as f:
        return yaml.safe_load(f)

def run_plugin(plugin_name: str, config_path: str, extra_args):
    """Loads and executes a plugin dynamically."""
    from devtools.common.session import TraceSession
    
    config = load_config(config_path) if config_path else {}
    session = TraceSession(config)

    # Load symbol map if specified in config
    map_path = config.get("kernel", {}).get("symbol_map")
    if map_path and os.path.exists(map_path):
        session.symbols.load_map(map_path)

    # 1. Connect GDB stub
    gdb_host = config.get("gdb", {}).get("host", "127.0.0.1")
    gdb_port = config.get("gdb", {}).get("port", 1234)
    try:
        session.connect(gdb_host, gdb_port)
    except Exception as e:
        logger.error(f"Failed to connect to GDB server: {e}")
        sys.exit(1)

    # 2. Locate and import the plugin class dynamically
    plugin_class = None
    try:
        # Search internal plugin directory first
        internal_mod_name = f"devtools.plugins.{plugin_name}"
        mod = importlib.import_module(internal_mod_name)
        # Find subclass of Plugin in module
        from devtools.plugins.base import Plugin
        for attr_name in dir(mod):
            attr = getattr(mod, attr_name)
            if isinstance(attr, type) and issubclass(attr, Plugin) and attr is not Plugin:
                plugin_class = attr
                break
    except ImportError:
        pass

    if not plugin_class:
        logger.error(f"Could not load plugin: {plugin_name}")
        session.disconnect()
        sys.exit(1)

    logger.info(f"Running plugin: {plugin_name}...")
    plugin_instance = plugin_class(session)
    
    try:
        exit_code = plugin_instance.run(extra_args)
    except Exception as e:
        logger.error(f"Plugin execution failed with exception: {e}", exc_info=True)
        exit_code = 1
    finally:
        session.disconnect()

    logger.info(f"Plugin completed with exit code: {exit_code}")
    sys.exit(exit_code)

def main():
    parser = argparse.ArgumentParser(description="DevTools CLI Runner for CustomAndroidEmulator")
    subparsers = parser.add_subparsers(dest="command", required=True)

    # Run Plugin command
    run_parser = subparsers.add_parser("run", help="Run a debugger analysis plugin")
    run_parser.add_argument("plugin", type=str, help="Name of the plugin to run")
    run_parser.add_argument("--config", type=str, required=True, help="Path to config YAML file")

    args, extra_args = parser.parse_known_args()

    if args.command == "run":
        run_plugin(args.plugin, args.config, extra_args)

if __name__ == "__main__":
    main()
