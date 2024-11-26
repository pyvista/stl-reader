"""Read a STL file using a wrapper of https://github.com/aki5/libstl."""

from typing import TYPE_CHECKING, Tuple

import numpy as np
import numpy.typing as npt

from stl_reader import stl_reader as _stlfile_wrapper

if TYPE_CHECKING:
    from pyvista.core.pointset import PolyData


def _polydata_from_faces(points: npt.NDArray[float], faces: npt.NDArray[int]) -> "PolyData":
    """Generate a polydata from a faces array containing no padding and all triangles.

    This is a more efficient way of instantiating PolyData from point and face
    data.

    Parameters
    ----------
    points : np.ndarray
        Points array.
    faces : np.ndarray
        ``(n, 3)`` faces array.

    """
    try:
        from pyvista.core.pointset import PolyData
    except ModuleNotFoundError:
        raise ModuleNotFoundError(
            "To use this functionality, install PyVista with:\n\npip install pyvista"
        )

    from pyvista import ID_TYPE, CellArray

    if faces.ndim != 2:
        raise ValueError("Expected a two dimensional face array.")

    # zero copy polydata creation
    offset = np.arange(0, faces.size + 1, faces.shape[1], dtype=ID_TYPE)
    pdata = PolyData()
    pdata.points = points
    pdata.faces = CellArray.from_arrays(offset, faces)

    return pdata


def read(filename: str) -> Tuple[npt.NDArray[np.float32], npt.NDArray[np.uint32]]:
    """
    Read a binary STL file and returns the vertices and points.

    Parameters
    ----------
    filename : str
        The path to the binary STL file.

    Returns
    -------
    vertices : np.ndarray
        The vertices from the STL file, as a 2D NumPy array. Each row of
        the array represents a vertex, with the three columns
        representing the X, Y, and Z coordinates, respectively.
    indices : np.ndarray
        The indices representing the triangles from the STL file.

    Raises
    ------
    FileNotFoundError
        If the specified STL file does not exist.
    RuntimeError
        If the STL file is not valid or cannot be read.

    Example
    -------
    >>> import stl_reader
    >>> vertices, indices = stl_reader.read("example.stl")
    >>> vertices
    array([[-0.01671113,  0.5450843 , -0.8382146 ],
           [ 0.01671113,  0.5450843 , -0.8382146 ],
           [ 0.        ,  0.52573115, -0.8506509 ],
           ...,
           [ 0.5952229 , -0.57455426,  0.56178033],
           [ 0.56178033, -0.5952229 ,  0.57455426],
           [ 0.57455426, -0.56178033,  0.5952229 ]], dtype=float32)
    >>> indices
    array([[      0,       1,       2],
           [      1,       3,       4],
           [      4,       5,       2],
           ...,
           [9005998, 9005988, 9005999],
           [9005999, 9005996, 9005995],
           [9005998, 9005999, 9005995]], dtype=uint32)

    """
    return _stlfile_wrapper.get_stl_data(filename)


def read_as_mesh(filename: str) -> "PolyData":
    """
    Read a binary STL file and return it as a mesh.

    This function uses the `get_stl_data` function, which is a wrapper
    of https://github.com/aki5/libstl, to read STL files.

    Parameters
    ----------
    filename : str
        The path to the binary STL file.

    Returns
    -------
    mesh : pyvista.PolyData
        The mesh from the STL file, represented as a PyVista PolyData object.

    Raises
    ------
    FileNotFoundError
        If the specified STL file does not exist.
    RuntimeError
        If the STL file is not valid or cannot be read.

    Example
    -------
    >>> import stl_reader
    >>> mesh = stl_reader.read_as_mesh("example.stl")
    >>> mesh
    PolyData (0x7f43063ec700)
      N Cells:    1280000
      N Points:   641601
      N Strips:   0
      X Bounds:   -5.000e-01, 5.000e-01
      Y Bounds:   -5.000e-01, 5.000e-01
      Z Bounds:   -5.551e-17, 5.551e-17
      N Arrays:   0

    Notes
    -----
    Requires the ``pyvista`` library.

    """
    try:
        from pyvista import ID_TYPE
    except ModuleNotFoundError:
        raise ModuleNotFoundError(
            "To use this functionality, install PyVista with:\n\npip install pyvista"
        )
    vertices, indices = read(filename)
    indices_int = indices.astype(ID_TYPE, copy=False)
    return _polydata_from_faces(vertices, indices_int)
