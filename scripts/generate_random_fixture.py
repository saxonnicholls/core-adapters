#!/usr/bin/env python3
#
#  generate_random_fixture.py
#  core-adapters
#
#  Copyright 2010-2026 Saxon Herschel Nicholls
#
#  Writes tests/fixtures/wyrand_sequences.json: draws of wyhash's wyrand (the published generator of wyhash
#  final 4.2: state += 0x2d358dccaa6c78a5; return mix(state, state ^ 0x8bb84b93962eacc9), mix = the 128-bit product's
#  halves xored), splitmix64 streams, Lemire's unbiased uniform_index, uniform_real and the Fisher-Yates shuffle -
#  computed here in plain Python integers, independently of the C++. The C++ tests (core itself) and the Python
#  tests (the binding) both compare against this file, so all three agree bit for bit.
#
#      scripts/generate_random_fixture.py           rewrite the fixture
#      scripts/generate_random_fixture.py --check   exit 1 if the fixture differs (CTest generated.random_fixture)
#

import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FIXTURE = os.path.join(ROOT, "tests", "fixtures", "wyrand_sequences.json")
M64 = (1 << 64) - 1


def mix(a, b):
    p = a * b
    return (p & M64) ^ (p >> 64)


def splitmix64(x):
    x = (x + 0x9E3779B97F4A7C15) & M64
    x = ((x ^ (x >> 30)) * 0xBF58476D1CE4E5B9) & M64
    x = ((x ^ (x >> 27)) * 0x94D049BB133111EB) & M64
    return x ^ (x >> 31)


class Wyrand:
    def __init__(self, seed):
        self.state = seed & M64

    @staticmethod
    def stream(seed, k):
        return Wyrand(splitmix64(seed ^ splitmix64((k + 0x9E3779B97F4A7C15) & M64)))

    def __call__(self):
        self.state = (self.state + 0x2D358DCCAA6C78A5) & M64
        return mix(self.state, self.state ^ 0x8BB84B93962EACC9)


def uniform_index(g, n):
    m = g() * n
    low, high = m & M64, m >> 64
    if low < n:
        threshold = ((1 << 64) - n) % n
        while low < threshold:
            m = g() * n
            low, high = m & M64, m >> 64
    return high


def uniform_real(g):
    return (g() >> 11) * 2.0 ** -53


def shuffle(values, g):
    for i in range(len(values), 1, -1):
        j = uniform_index(g, i)
        values[i - 1], values[j] = values[j], values[i - 1]
    return values


def build():
    draws = {}
    for seed in (0, 1, 42, M64):
        g = Wyrand(seed)
        draws[str(seed)] = [g() for _ in range(8)]
    streams = []
    for seed, k in ((7, 0), (7, 1), (7, 2), (0, 0), (123456789, 1000)):
        g = Wyrand.stream(seed, k)
        streams.append({"seed": seed, "k": k, "draws": [g() for _ in range(4)]})
    indices = []
    for seed, n in ((3, 1), (3, 7), (11, 10), (5, 1 << 32), (9, (1 << 63) + 1), (13, M64)):
        g = Wyrand(seed)
        indices.append({"seed": seed, "n": n, "draws": [uniform_index(g, n) for _ in range(16)]})
    g = Wyrand(9)
    reals = {"seed": 9, "draws": [uniform_real(g) for _ in range(8)]}
    shuffles = []
    for seed, size in ((5, 10), (17, 1), (17, 2), (99, 32)):
        shuffles.append({"seed": seed, "size": size, "result": shuffle(list(range(size)), Wyrand(seed))})
    return {
        "description": "wyhash's wyrand (final 4.2), splitmix64 streams, Lemire's uniform_index, uniform_real and "
                       "the Fisher-Yates shuffle, computed independently in Python integers by "
                       "scripts/generate_random_fixture.py. Integers are decimal; reals are exact binary64 values.",
        "draws": draws, "streams": streams, "uniform_index": indices, "uniform_real": reals, "shuffle": shuffles,
    }


def main():
    text = json.dumps(build(), indent=1) + "\n"
    if "--check" in sys.argv:
        current = open(FIXTURE, encoding="utf-8").read() if os.path.exists(FIXTURE) else ""
        if current != text:
            print("generate_random_fixture: tests/fixtures/wyrand_sequences.json is out of date", file=sys.stderr)
            return 1
        print("generate_random_fixture: up to date")
        return 0
    open(FIXTURE, "w", encoding="utf-8").write(text)
    print(f"generate_random_fixture: wrote {os.path.relpath(FIXTURE, ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
