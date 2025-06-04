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

    from vtkmodules.util.numpy_support import numpy_to_vtk as numpy_to_vtk
    from vtkmodules.vtkCommonCore import vtkTypeInt32Array, vtkTypeInt64Array
    from vtkmodules.vtkCommonDataModel import vtkCellArray

    if faces.ndim != 2:
        raise ValueError("Expected a two dimensional face array.")

    if faces.dtype == np.int32:
        vtk_dtype = vtkTypeInt32Array().GetDataType()
    elif faces.dtype == np.int64:
        vtk_dtype = vtkTypeInt64Array().GetDataType()
    else:
        raise TypeError(f"Unsupported dtype ({type(faces)} for faces. Expected int32 or int64.")

    # convert to vtk arrays without copying
    offset = np.arange(0, faces.size + 1, faces.shape[1], dtype=faces.dtype)
    offset_vtk = numpy_to_vtk(offset, deep=False, array_type=vtk_dtype)
    faces_vtk = numpy_to_vtk(faces.ravel(), deep=False, array_type=vtk_dtype)

    # create the vtk arrays and keep references to avoid gc
    carr = vtkCellArray()
    carr.SetData(offset_vtk, faces_vtk)
    carr._offset_np_ref = offset_vtk
    carr._faces_np_ref = faces_vtk

    pdata = PolyData()
    pdata.points = points
    pdata.SetPolys(carr)
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
    vertices, indices = read(filename)

    # while faces might be correctly sized, the required offset might exceed np.int32
    dtype = np.int64 if indices.size >= np.iinfo(np.int32).max else np.int32

    # check if we can support int32 conversion
    if vertices.shape[0] > np.iinfo(np.int32).max:
        dtype = np.int64

    indices_int = indices.astype(dtype, copy=False)
    return _polydata_from_faces(vertices, indices_int)  # type: ignore
