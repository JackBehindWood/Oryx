from pathlib import Path
import json

PROJECT_ROOT = Path(__file__).resolve().parent.parent

PREMAKE_DIR = PROJECT_ROOT / "premake"
BIN_DIR = PROJECT_ROOT / "bin"

CONFIG_FILE = PROJECT_ROOT / "build_config.json"

def default_config(): 
    """Return the default build configuration.""" 
    return { 
        "project_name": "oryx", 
        "build_generator": "gmake2", 
        "test_executable": "oryx_tests", 
        }


def load_config(): 
    """Load the project build configuration.""" 
    if not CONFIG_FILE.exists(): 
        raise FileNotFoundError( f"Build configuration not found: {CONFIG_FILE}" ) 

    with CONFIG_FILE.open("r", encoding="utf-8") as file: 
        return json.load(file) 

def save_config(config): 
    """Save the project build configuration.""" 
    with CONFIG_FILE.open("w", encoding="utf-8") as file: 
        json.dump(config, file, indent=2) 
        file.write("\n")
