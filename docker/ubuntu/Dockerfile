# syntax=docker/dockerfile:1.6
#
# core-adapters - the Ubuntu build and test image, after core's (docker/ubuntu/Dockerfile in core): Ubuntu 24.04,
# GCC 13, Clang 18 with compiler-rt, CMake, Ninja, plus what the Python package needs - Python 3.12 with its
# development files and venv, and a wheelhouse of the pinned packages (requirements/gate.txt) so that
# scripts/gate.sh runs offline inside the container. The build context is requirements/ (scripts/test_ubuntu.sh).

FROM ubuntu:24.04

LABEL org.opencontainers.image.title="core-adapters Ubuntu builder" \
      org.opencontainers.image.description="Ubuntu 24.04: GCC 13, Clang 18, CMake, Ninja, Python 3.12 + wheelhouse"

ENV DEBIAN_FRONTEND=noninteractive

RUN --mount=type=cache,target=/var/cache/apt \
    --mount=type=cache,target=/var/lib/apt \
    apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential g++-13 \
      clang-18 libclang-rt-18-dev \
      cmake ninja-build \
      python3.12 python3.12-dev python3.12-venv python3-pip \
      git ca-certificates && \
    ln -sf /usr/bin/clang-18 /usr/local/bin/clang && \
    ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++ && \
    rm -rf /var/lib/apt/lists/*

# The wheelhouse: downloaded once, at image build time; the gate itself never touches the network
COPY gate.txt /opt/requirements/gate.txt
RUN python3.12 -m pip download --quiet -d /opt/wheelhouse -r /opt/requirements/gate.txt && \
    ls /opt/wheelhouse
ENV CORE_ADAPTERS_WHEELHOUSE=/opt/wheelhouse

WORKDIR /work
CMD ["bash"]
