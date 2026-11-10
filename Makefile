# core-adapters - the short way to drive CMake and the scripts.
#
#   make gate        scripts/gate.sh: the repository's gate (Debug + Release, ctest, pip install ., pytest), offline
#   make test        configure, build and run every CTest in build/ (Release, with the gate's development venv)
#   make ubuntu      the gate on Ubuntu 24.04 in Docker (scripts/test_ubuntu.sh)
#   make docs        Doxygen, warnings are errors; output build-docs/docs/html
#   make check       the static checks: version and mirror
#   make demos       every demo, output shown (scripts/run_demos.sh)
#   make benchmarks  Release benchmarks; rewrites BENCHMARKS.md
#   make wheels      fetch the pinned Python packages into the wheelhouse (the one step that uses the network)

BUILD ?= build
PY    ?= $(CURDIR)/build-family/venv-dev/bin/python

.PHONY: gate test ubuntu docs check demos benchmarks wheels clean

gate:
	scripts/gate.sh

test:
	cmake -S . -B $(BUILD) -G Ninja -DCMAKE_BUILD_TYPE=Release -DPython_EXECUTABLE=$(PY) -DPython3_EXECUTABLE=$(PY)
	cmake --build $(BUILD) -j 4
	ctest --test-dir $(BUILD) -j 4 --output-on-failure

ubuntu:
	scripts/test_ubuntu.sh

docs:
	cmake -S . -B build-docs -DCORE_ADAPTERS_BUILD_TESTS=OFF -DCORE_ADAPTERS_WITH_NANOBIND=OFF -DCORE_ADAPTERS_BUILD_DOCS=ON
	cmake --build build-docs --target docs

check:
	python3 scripts/check_version.py
	python3 scripts/check_mirror.py

demos:
	scripts/run_demos.sh

benchmarks:
	scripts/run_benchmarks.sh

wheels:
	scripts/fetch_wheels.sh

clean:
	rm -rf build build-*
