"""Test stl_reader."""
import numpy as np
import pytest
import pyvista as pv
import stl_reader


@pytest.fixture
def stlfile(tmpdir):
    filename = tmpdir.join("tmp.stl")
    pv.Sphere().save(filename)
    return str(filename)


@pytest.fixture
def stlfile_ascii(tmpdir):
    filename = tmpdir.join("tmp.stl")
    pv.Sphere().save(filename, binary=False)
    return str(filename)


def test_read_binary(stlfile):
    pv_mesh = pv.read(stlfile)

    points, ind = stl_reader.read(stlfile)
    assert np.allclose(pv_mesh.points, points)
    assert np.allclose(pv_mesh._connectivity_array, ind.ravel())


def test_read_ascii(stlfile_ascii):
    with pytest.raises(RuntimeError):
        stl_reader.read(stlfile_ascii)


def test_read_as_mesh(stlfile):
    pv_mesh = pv.read(stlfile)

    stl_mesh = stl_reader.read_as_mesh(stlfile)
    assert pv_mesh == stl_mesh
