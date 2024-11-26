############
 stl-reader
############

|pypi| |MIT|

.. |pypi| image:: https://img.shields.io/pypi/v/stl-reader.svg?logo=python&logoColor=white
   :target: https://pypi.org/project/stl-reader/

.. |MIT| image:: https://img.shields.io/badge/License-MIT-yellow.svg
   :target: https://opensource.org/licenses/MIT

``stl-reader`` is a Python library for rapidly reading binary and ASCII
STL files. It wraps a `nanobind
<https://nanobind.readthedocs.io/en/latest/>`_ interface to the fast STL
library provided by `libstl <https://github.com/aki5/libstl>`_. Thanks
@aki5!

The main advantage of ``stl-reader`` over other STL reading libraries is
its performance. It is particularly well-suited for large files, mainly
due to its efficient use of hashing when merging points. This results in
a 5-35x speedup over VTK for files containing between 4,000 and
9,000,000 points.

See the benchmarks below for more details.

**************
 Installation
**************

The recommended way to install ``stl-reader`` is via PyPI:

.. code:: sh

   pip install stl-reader

You can also clone the repository and install it from source:

.. code:: sh

   git clone https://github.com/pyvista/stl-reader.git
   cd stl-reader
   pip install .

*******
 Usage
*******

Load in the vertices and indices of a STL file directly as a NumPy
array:

.. code:: pycon

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

In this example, ``vertices`` is a 2D NumPy array where each row
represents a vertex and the three columns represent the X, Y, and Z
coordinates, respectively. ``indices`` is a 1D NumPy array representing
the triangles from the STL file.

Alternatively, you can load in the STL file as a PyVista PolyData:

.. code:: pycon

   >>> import stl_reader
   >>> mesh = stl_reader.read_as_mesh('example.stl')
   >>> mesh
   PolyData (0x7f43063ec700)
     N Cells:    1280000
     N Points:   641601
     N Strips:   0
     X Bounds:   -5.000e-01, 5.000e-01
     Y Bounds:   -5.000e-01, 5.000e-01
     Z Bounds:   -5.551e-17, 5.551e-17
     N Arrays:   0

***********
 Benchmark
***********

The main reason behind writing yet another STL file reader for Python is
to leverage the performant `libstl <https://github.com/aki5/libstl>`_
library.

Here are some timings from reading in a 1,000,000 point binary STL file:

+-------------+-----------------------+
| Library     | Time (seconds)        |
+=============+=======================+
| stl-reader  | 0.174                 |
+-------------+-----------------------+
| numpy-stl   | 0.201 (see note)      |
+-------------+-----------------------+
| PyVista     | 1.663                 |
| (VTK)       |                       |
+-------------+-----------------------+
| meshio      | 4.451                 |
+-------------+-----------------------+

**Note** ``numpy-stl`` does not merge duplicate vertices.

Comparison with VTK
===================

Here's an additional benchmark comparing VTK with ``stl-reader``:

.. code:: python

   import numpy as np
   import time
   import pyvista as pv
   import matplotlib.pyplot as plt
   import stl_reader

   times = []
   filename = 'tmp.stl'
   for res in range(50, 800, 50):
       mesh = pv.Plane(i_resolution=res, j_resolution=res).triangulate().subdivide(2)
       mesh.save(filename)

       tstart = time.time()
       out_pv = pv.read(filename)
       vtk_time = time.time() - tstart

       tstart = time.time()
       out_stl = stl_reader.read(filename)
       stl_reader_time =  time.time() - tstart

       times.append([mesh.n_points, vtk_time, stl_reader_time])
       print(times[-1])


   times = np.array(times)
   plt.figure(1)
   plt.title('STL load time')
   plt.plot(times[:, 0], times[:, 1], label='VTK')
   plt.plot(times[:, 0], times[:, 2], label='stl_reader')
   plt.xlabel('Number of Points')
   plt.ylabel('Time to Load (seconds)')
   plt.legend()

   plt.figure(2)
   plt.title('STL load time (Log-Log)')
   plt.loglog(times[:, 0], times[:, 1], label='VTK')
   plt.loglog(times[:, 0], times[:, 2], label='stl_reader')
   plt.xlabel('Number of Points')
   plt.ylabel('Time to Load (seconds)')
   plt.legend()
   plt.show()

.. image:: https://github.com/pyvista/stl-reader/raw/main/bench0.png

.. image:: https://github.com/pyvista/stl-reader/raw/main/bench1.png

Read in ASCII Meshes
====================

The `stl-reader` also supports ASCII files and is around 2.4 times
faster than VTK at reading ASCII files.

.. code:: python

   import time
   import stl_reader
   import pyvista as pv
   import numpy as np

   # create and save a ASCII file
   n = 1000
   mesh = pv.Plane(i_resolution=n, j_resolution=n).triangulate()
   mesh.save("/tmp/tmp-ascii.stl", binary=False)

   # stl reader
   tstart = time.time()
   mesh = stl_reader.read_as_mesh("/tmp/tmp-ascii.stl")
   print("stl-reader    ", time.time() - tstart)

   tstart = time.time()
   pv_mesh = pv.read("/tmp/tmp-ascii.stl")
   print("pyvista reader", time.time() - tstart)

   # verify meshes are identical
   assert np.allclose(mesh.points, pv_mesh.points)

   # approximate time to read in the 1M point file:
   # stl-reader     0.80303955078125
   # pyvista reader 1.916085958480835

*****************************
 License and Acknowledgments
*****************************

This project relies on `libstl <https://github.com/aki5/libstl>`_ for
reading in and merging the vertices of a STL file. Wherever code is
reused, the original `MIT License
<https://github.com/aki5/libstl/blob/master/LICENSE>`_ is mentioned.

The work in this repository is also licensed under the MIT License.

*********
 Support
*********

If you are having issues, please feel free to raise an `Issue
<https://github.com/pyvista/stl-reader/issues>`_.
