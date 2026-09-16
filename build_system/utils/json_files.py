import json
from pathlib import Path


def load_json(path: Path, default: dict) -> dict:
    """Load a JSON file, or return a copy of `default` if it doesn't exist."""
    if not path.exists():
        return json.loads(json.dumps(default))  # cheap deep copy
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        raise ValueError(f"Existing {path} is not valid JSON: {error}") from error


def write_json(path: Path, data) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=4) + "\n", encoding="utf-8")
    return path


def merge_by_key(existing_items: list, generated_items: list, key: str) -> list:
    """Replace items (matched by `key`) that this CLI generates; keep the rest.

    This is what lets us regenerate our own entries on every run without
    clobbering anything the user added by hand alongside them.
    """
    generated_keys = {item[key] for item in generated_items}
    kept = [item for item in existing_items if item.get(key) not in generated_keys]
    return kept + generated_items
