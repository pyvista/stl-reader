"""Stl reader library."""

from importlib.metadata import PackageNotFoundError, version

from stl_reader.reader import read, read_as_mesh

try:
    __version__ = version("stl_reader")
except PackageNotFoundError:
    __version__ = "unknown"


__all__ = ["read", "read_as_mesh", "__version__"]
