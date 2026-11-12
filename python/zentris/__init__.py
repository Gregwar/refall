"""Refall: a music-reactive falling-block puzzle game that plays any playlist."""

from importlib.metadata import PackageNotFoundError, version

try:
    __version__ = version("refall")  # single source of truth: pyproject.toml
except PackageNotFoundError:
    __version__ = "unknown"
