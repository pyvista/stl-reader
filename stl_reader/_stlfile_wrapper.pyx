
# Import the Python-level symbols of numpy
import numpy as np

cimport numpy as np

# Numpy must be initialized. When using numpy from C or Cython you must
# _always_ do that, or you will have segfaults
np.import_array()

cimport cython
from libc.stdio cimport FILE, fclose, fopen
from libc.stdlib cimport free, malloc


cdef extern from "stlfile.h":
    ctypedef unsigned int vertex_t
    ctypedef unsigned int triangle_t
    int loadstl(FILE *fp, char *comment, float **vertp, vertex_t *nvertp, vertex_t **trip, unsigned short **attrp, triangle_t *ntrip)


cdef class ArrayWrapper:
    cdef void* data_ptr
    cdef int size
    cdef int dtype

    cdef set_data(self, int size, void* data_ptr, int dtype):
        """ Constructor for the class.
        Mallocs a memory buffer of size (n*sizeof(int)) and sets up
        the numpy array.
        Parameters:
        -----------
        n -- Length of the array.
        Data attributes:
        ----------------
        data -- Pointer to an integer array.
        alloc -- Size of the data buffer allocated.
        """
        self.data_ptr = data_ptr
        self.size = size
        self.dtype = dtype

    def __array__(self):
        cdef np.npy_intp shape[1]
        shape[0] = <np.npy_intp> self.size
        ndarray = np.PyArray_SimpleNewFromData(1, shape, self.dtype, self.data_ptr)
        return ndarray

    def __dealloc__(self):
        """ Frees the array. """
        free(<void*>self.data_ptr)


@cython.boundscheck(False)
@cython.wraparound(False)
def get_stl_data(str filename):
    cdef:
        FILE *fp
        char comment[80]
        float *vertp
        vertex_t nverts
        vertex_t *trip
        unsigned short *attrp
        triangle_t ntrip
        np.ndarray verts_arr, tri_arr, attr_arr
        int out

    fp = fopen(filename.encode('utf-8'), "rb")
    if fp is NULL:
        raise FileNotFoundError(2, "No such file or directory: '%s'" % filename)

    # Call the STL file loader
    out = loadstl(fp, comment, &vertp, &nverts, &trip, &attrp, &ntrip)
    fclose(fp)
    if out == -2:
        raise RuntimeError("Invalid or unrecognized STL file format.")
    elif out != 0:
        raise RuntimeError("Unable to load STL.")

    # wrap the vertex array
    point_array_wrapper = ArrayWrapper()
    point_array_wrapper.set_data(nverts*3, <void*> vertp, np.NPY_FLOAT32)
    points = np.array(point_array_wrapper).reshape(nverts, 3)

    # wrap the vertex array
    tri_array_wrapper = ArrayWrapper()
    tri_array_wrapper.set_data(ntrip*3, <void*> trip, np.NPY_UINT32)
    indices = np.array(tri_array_wrapper).reshape(ntrip, 3)

    return points, indices
