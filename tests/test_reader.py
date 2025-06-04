"""Test stl_reader."""

import os
import numpy as np
import pytest

import stl_reader


try:
    import pyvista as pv

    PYVISTA_INSTALLED = True
except ImportError:
    PYVISTA_INSTALLED = False

THIS_PATH = os.path.dirname(os.path.abspath(__file__))
TEST_FILE_ASCII = os.path.join(THIS_PATH, "sphere_ascii.stl")
TEST_FILE_BINARY = os.path.join(THIS_PATH, "sphere_binary.stl")

EXPECTED_POINTS = np.array(
    [
        [0.0, 0.52573115, -0.8506509],
        [-0.52573115, 0.8506509, 0.0],
        [0.52573115, 0.8506509, 0.0],
        [0.0, 0.52573115, 0.8506509],
        [-0.8506509, 0.0, 0.52573115],
        [0.0, -0.52573115, 0.8506509],
        [0.8506509, 0.0, 0.52573115],
        [0.8506509, 0.0, -0.52573115],
        [0.0, -0.52573115, -0.8506509],
        [-0.8506509, 0.0, -0.52573115],
        [-0.52573115, -0.8506509, 0.0],
        [0.52573115, -0.8506509, 0.0],
    ],
    dtype=np.float32,
)


EXPECTED_FACES = np.array(
    [
        [0, 1, 2],
        [3, 2, 1],
        [3, 4, 5],
        [3, 5, 6],
        [0, 7, 8],
        [0, 8, 9],
        [5, 10, 11],
        [8, 11, 10],
        [1, 9, 4],
        [10, 4, 9],
        [2, 6, 7],
        [11, 7, 6],
        [3, 1, 4],
        [3, 6, 2],
        [0, 9, 1],
        [0, 2, 7],
        [8, 10, 9],
        [8, 7, 11],
        [5, 4, 10],
        [5, 11, 6],
    ],
    dtype=np.uint32,
)


def test_read_binary() -> None:
    points, ind = stl_reader.read(TEST_FILE_BINARY)
    assert np.allclose(EXPECTED_POINTS, points)
    assert np.allclose(EXPECTED_FACES, ind)


def test_read_ascii() -> None:
    points, ind = stl_reader.read(TEST_FILE_ASCII)


@pytest.mark.skipif(not PYVISTA_INSTALLED, reason="Requires PyVista")  # type: ignore
def test_read_as_mesh() -> None:
    pv_mesh = pv.read(TEST_FILE_BINARY)

    stl_mesh = stl_reader.read_as_mesh(TEST_FILE_BINARY)
    assert pv_mesh == stl_mesh
    assert stl_mesh._connectivity_array.dtype == np.int32
