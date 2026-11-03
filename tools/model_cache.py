"""Resolve the user-local cache for optional wake-model assets."""

from pathlib import Path
import os
import sys


def model_cache_root() -> Path:
    """Return the cache root; LX06_MODEL_CACHE can override the platform default."""
    override = os.environ.get("LX06_MODEL_CACHE")
    if override:
        return Path(override).expanduser()
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Caches" / "xiaoai-sidekick"
    if os.name == "nt":
        local = Path(os.environ.get("LOCALAPPDATA", Path.home() / "AppData" / "Local"))
        return local / "xiaoai-sidekick"
    cache_home = Path(os.environ.get("XDG_CACHE_HOME", Path.home() / ".cache"))
    return cache_home / "xiaoai-sidekick"
