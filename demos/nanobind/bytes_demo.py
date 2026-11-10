#
#  bytes_demo.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
"""Demo for <snicholls/core_adapters/nanobind/bytes.hpp>: bytes, bytearray, memoryview and str as C++ views."""

import sys

import bytes_demo_module as m

h = [m.fnv1a_64(x) for x in (b"abc", bytearray(b"abc"), memoryview(b"xabc")[1:])]
print("fnv1a_64 of b'abc' three ways:", [hex(x) for x in h])
print(m.shout("héllo"), m.shout(b"bytes"))
buf = bytearray(b"secret")
m.zero(buf)
print("zeroed in place:", buf)
print("header(258) =", m.header(258))
try:
    m.fnv1a_64("a str has no bytes until it is encoded")
    refused = False
except TypeError:
    refused = True
print("a str is refused as bytes:", refused)
ok = (h[0] == h[1] == h[2] == 0xE71FA2190541574B and m.shout("héllo") == "héllo!" and buf == bytearray(6)
      and m.header(258) == b"\x00\x00\x01\x02" and refused)
print("bytes_demo:", "ok" if ok else "FAILED")
sys.exit(0 if ok else 1)
