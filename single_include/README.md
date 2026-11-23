# The single include

`snicholls_core_adapters.hpp` is the part of core-adapters that links no third party, in one file:
`include/snicholls/core_adapters/core_adapters.hpp` with every header of its own and of snicholls-core expanded in
place, once each, needing nothing but the standard library. Every adapter needs its third-party library (nanobind and
Python, nlohmann/json), so the adapters are not in it: include them through their CMake targets.

- **Download it** from the [release](https://github.com/saxonnicholls/core-adapters/releases) for the version you pin.
  Each release's file is generated from that release's tagged commit and compiled on its own before it is attached.
- **Build it** for any other commit with `make single-include`, which writes it here (git-ignored), or with
  `python3 third_party/tooling/scripts/single_include.py --out <dir>`.

It is never committed, so it is never stale: every build generates it and compiles it alone
(`core_adapters_single_include`), and the CTest `check.single_include` fails if a header is missing from the umbrella.
