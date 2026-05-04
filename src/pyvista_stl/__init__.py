"""pyvista-stl reader library."""

from importlib.metadata import PackageNotFoundError, version

from pyvista_stl.reader import read, read_as_mesh

try:
    __version__ = version("pyvista-stl")
except PackageNotFoundError:
    __version__ = "unknown"


__all__ = ["read", "read_as_mesh", "__version__"]
