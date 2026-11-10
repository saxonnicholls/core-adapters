#
#  ndarray_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""<snicholls/core_adapters/nanobind/ndarray.hpp>: arrays in, spans, arrays out without copies."""

import gc
import sys

import ndarray_module as m
import numpy as np
import pytest
from snicholls_core.errors import PreconditionError


def test_vector_in_reads_numpy_and_converts_other_dtypes_and_strides():
    assert m.sum(np.arange(5, dtype=np.float64)) == 10.0
    assert m.sum(np.arange(5, dtype=np.int32)) == 10.0           # converted by nanobind
    assert m.sum(np.arange(10, dtype=np.float64)[::2]) == 20.0   # non-contiguous: copied first
    assert m.sum(np.zeros(0)) == 0.0


def test_noconvert_refuses_instead_of_copying():
    assert m.sum_strict(np.ones(3)) == 3.0
    with pytest.raises(TypeError):
        m.sum_strict(np.ones(3, dtype=np.float32))
    with pytest.raises(TypeError):
        m.sum_strict(np.ones(6)[::2])


def test_wrong_rank_is_a_type_error():
    with pytest.raises(TypeError):
        m.sum(np.ones((2, 2)))
    with pytest.raises(TypeError):
        m.matrix_row_sums(np.ones(3))


def test_matrix_in_is_row_major():
    a = np.arange(6, dtype=np.float64).reshape(2, 3)
    np.testing.assert_array_equal(m.matrix_row_sums(a), [3.0, 12.0])
    np.testing.assert_array_equal(m.matrix_row_sums(np.asfortranarray(a)), [3.0, 12.0])


def test_inout_writes_in_place():
    a = np.arange(4, dtype=np.float64)
    m.scale(a, 2.0)
    np.testing.assert_array_equal(a, [0, 2, 4, 6])
    with pytest.raises(TypeError):
        m.scale(np.arange(4, dtype=np.float64)[::2], 2.0)


def test_to_numpy_hands_over_the_buffer():
    a = m.iota(5)
    assert a.dtype == np.int64 and a.shape == (5,) and a.flags.c_contiguous and a.flags.writeable
    np.testing.assert_array_equal(a, np.arange(5))
    g = m.grid(2, 3)
    assert g.shape == (2, 3) and g[1, 2] == 5.0
    assert m.iota(0).shape == (0,)


def test_the_result_owns_its_memory():
    a = m.iota(1000)
    view = a[10:20]
    del a
    gc.collect()
    np.testing.assert_array_equal(view, np.arange(10, 20))


def test_shape_mismatch_is_a_value_error():
    with pytest.raises(ValueError, match="rows \\* columns"):
        m.bad_grid()


def test_the_documented_template():
    v = np.array([0.0, 1.0, 2.0, 4.0])                # v(1) = 1, v(2) = 2, v(12) = 4: dividend of {1, 2} is 1
    np.testing.assert_allclose(m.harsanyi_dividends(v), [0.0, 1.0, 2.0, 1.0])
    with pytest.raises(PreconditionError):
        m.harsanyi_dividends(np.ones(3))


def test_no_reference_leaks_on_repeated_calls():
    x = np.ones(100)
    before = sys.getrefcount(x)
    for _ in range(1000):
        m.sum(x)
    assert sys.getrefcount(x) == before
