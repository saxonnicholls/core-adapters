#
#  bytes_test.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""<snicholls/core_adapters/nanobind/bytes.hpp>: bytes-like and text views, and bytes back."""

import array

import bytes_module as m
import numpy as np
import pytest


@pytest.mark.parametrize("value", [b"abc", bytearray(b"abc"), memoryview(b"abc"), array.array("B", b"abc"),
                                   np.frombuffer(b"abc", dtype=np.uint8)])
def test_byte_view_takes_every_contiguous_buffer(value):
    assert m.length(value) == 3
    assert m.checksum(value) == sum(b"abc")
    assert m.chars(value) == "abc"


def test_byte_view_sizes_count_bytes_not_elements():
    assert m.length(array.array("d", [1.0, 2.0])) == 16
    assert m.length(b"") == 0


def test_byte_view_refuses_str_and_non_buffers():
    with pytest.raises(TypeError):
        m.length("abc")
    with pytest.raises(TypeError):
        m.length(123)
    with pytest.raises(TypeError):
        m.length(memoryview(b"abcdef")[::2])   # not contiguous


def test_mutable_byte_view_writes_in_place_and_refuses_read_only():
    b = bytearray(b"hello")
    m.upper(b)
    assert b == bytearray(b"HELLO")
    mv = memoryview(bytearray(b"abc"))
    m.upper(mv)
    assert bytes(mv) == b"ABC"
    with pytest.raises(TypeError):
        m.upper(b"read only")


def test_a_view_pins_the_buffer_size_while_it_lives():
    b = bytearray(b"abc")
    m.length(b)      # released after the call
    b.extend(b"d")   # so resizing works again
    assert b == bytearray(b"abcd")


def test_text_view_takes_str_as_utf8_and_bytes_as_they_are():
    assert m.text("héllo") == "héllo"
    assert m.text_size("héllo") == 6
    assert m.text(b"plain") == "plain"
    assert m.text(memoryview(b"view")) == "view"
    with pytest.raises(TypeError):
        m.text(5)


def test_to_bytes_copies_into_new_bytes():
    assert m.reverse(b"abc") == b"cba"
    out = m.copy(bytearray(b"\x00\xff"))
    assert isinstance(out, bytes) and out == b"\x00\xff"
