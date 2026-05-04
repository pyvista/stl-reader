"""pyvista-stl reader library."""

from pyvista_stl.reader import read, read_as_mesh

try:
    from pyvista_stl._version import __version__
except ImportError:
    __version__ = "0.0.0"


__all__ = ["read", "read_as_mesh", "__version__"]
