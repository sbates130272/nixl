#!/bin/bash
# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
# SPDX-License-Identifier: MIT


# shellcheck disable=SC1091
. "$(dirname "$0")/../.ci/scripts/common.sh"

set -e
set -x
set -o pipefail

# Parse commandline arguments with first argument being the install directory
# and second argument being the UCX installation directory.
INSTALL_DIR=$1
UCX_INSTALL_DIR=$2
EXTRA_BUILD_ARGS=${3:-""}
NIXL_BUILD_DIR=${NIXL_BUILD_DIR:-nixl_build}
NIXLBENCH_BUILD_DIR=${NIXLBENCH_BUILD_DIR:-nixlbench_build}
# UCX_VERSION is the version of UCX to build override default with env variable.
UCX_VERSION=${UCX_VERSION:-v1.21.x}
# LIBFABRIC_VERSION is the version of libfabric to build override default with env variable.
LIBFABRIC_VERSION=${LIBFABRIC_VERSION:-v1.21.0}
# Abseil and gRPC versions for consistent toolchain build.
ABSL_TAG=${ABSL_TAG:-lts_2025_08_14}
GRPC_TAG=${GRPC_TAG:-v1.73.0}
# LIBFABRIC_INSTALL_DIR can be set via environment variable, defaults to INSTALL_DIR
LIBFABRIC_INSTALL_DIR=${LIBFABRIC_INSTALL_DIR:-$INSTALL_DIR}
# UCCL_COMMIT_SHA is the commit SHA of UCCL.
UCCL_COMMIT_SHA="0cdb740cf369a4f4dd63b9b773c8937f187b179a"
AZURITE_VER="3.35.0"
TMPDIR=$(mktemp -d)

# DEPS_SANITIZE, when set (e.g. "address"), builds the C++ dependency stack that
# shares Abseil's ABI with NIXL (abseil, protobuf/gRPC, etcd-cpp) using the
# matching -fsanitize flags. Required for AddressSanitizer: Abseil changes its
# SwissTable layout under ASan, so a prebuilt non-instrumented Abseil would
# mismatch NIXL's instrumented one at runtime (new-delete-type-mismatch during
# gRPC static init). Only ASan changes ABI (UBSan/TSan do not), so callers pass
# DEPS_SANITIZE=address. The array expands to nothing when unset.
DEPS_SANITIZE=${DEPS_SANITIZE:-""}
DEPS_SANITIZE_CMAKE_ARGS=()
if [ -n "$DEPS_SANITIZE" ]; then
    _deps_san_cxxflags="-fsanitize=${DEPS_SANITIZE}"
    case ",${DEPS_SANITIZE}," in
        # Abseil's headers hit a GCC constexpr bug under UBSan's null checks
        # (GCC #71962); drop those sub-checks if undefined is requested.
        *,undefined,*) _deps_san_cxxflags="${_deps_san_cxxflags} -fno-sanitize=null,nonnull-attribute,returns-nonnull-attribute" ;;
    esac
    DEPS_SANITIZE_CMAKE_ARGS=(
        "-DCMAKE_C_FLAGS=-fsanitize=${DEPS_SANITIZE}"
        "-DCMAKE_CXX_FLAGS=${_deps_san_cxxflags}"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${DEPS_SANITIZE}"
        "-DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=${DEPS_SANITIZE}"
    )
fi

if [ -z "$INSTALL_DIR" ]; then
    echo "Usage: $0 <install_dir> <ucx_install_dir>"
    exit 1
fi

if [ -z "$UCX_INSTALL_DIR" ]; then
    UCX_INSTALL_DIR=$INSTALL_DIR
fi


# For running as user - check if running as root, if not set sudo variable
if [ "$(id -u)" -ne 0 ]; then
    SUDO=sudo
else
    SUDO=""
fi

ARCH=$(uname -m)
[ "$ARCH" = "arm64" ] && ARCH="aarch64"

export LD_LIBRARY_PATH="${INSTALL_DIR}/lib:${INSTALL_DIR}/lib/$ARCH-linux-gnu:${INSTALL_DIR}/lib64:$LD_LIBRARY_PATH:${LIBFABRIC_INSTALL_DIR}/lib"
export CPATH="${INSTALL_DIR}/include:${LIBFABRIC_INSTALL_DIR}/include:$CPATH"
export PATH="${INSTALL_DIR}/bin:$HOME/.local/bin:/usr/local/bin:$HOME/.cargo/bin:$PATH"
export PKG_CONFIG_PATH="${INSTALL_DIR}/lib/pkgconfig:${INSTALL_DIR}/lib64/pkgconfig:${INSTALL_DIR}:${LIBFABRIC_INSTALL_DIR}/lib/pkgconfig:$PKG_CONFIG_PATH"
export NIXL_PLUGIN_DIR="${INSTALL_DIR}/lib/$ARCH-linux-gnu/plugins"
export CMAKE_PREFIX_PATH="${INSTALL_DIR}:${CMAKE_PREFIX_PATH}"

if [ -n "$PRE_INSTALLED_ENV" ]; then
    echo "PRE_INSTALLED_ENV is set, skipping package installation"
else
    # Some docker images are with broken installations:
    $SUDO rm -rf /usr/lib/cmake/grpc /usr/lib/cmake/protobuf

    $SUDO apt-get -qq update
    $SUDO apt-get -qq install -y python3-dev \
                                 python3-pip \
                                 curl \
                                 wget \
                                 libnuma-dev \
                                 numactl \
                                 autotools-dev \
                                 automake \
                                 git \
                                 libtool \
                                 libz-dev \
                                 libiberty-dev \
                                 flex \
                                 build-essential \
                                 cmake \
                                 libgoogle-glog-dev \
                                 libgtest-dev \
                                 libgmock-dev \
                                 libjsoncpp-dev \
                                 libpython3-dev \
                                 libboost-all-dev \
                                 libssl-dev \
                                 libprotobuf-dev \
                                 libcpprest-dev \
                                 libaio-dev \
                                 libelf-dev \
                                 libgflags-dev \
                                 patchelf \
                                 meson \
                                 ninja-build \
                                 parallel \
                                 pkg-config \
                                 protobuf-compiler-grpc \
                                 pybind11-dev \
                                 etcd-server \
                                 net-tools \
                                 iproute2 \
                                 pciutils \
                                 libpci-dev \
                                 uuid-dev \
                                 libibmad-dev \
                                 doxygen \
                                 clang \
                                 hwloc \
                                 libhwloc-dev \
                                 libxml2-dev \
                                 libcurl4-openssl-dev zlib1g-dev # aws-sdk-cpp dependencies
    $SUDO apt-mark hold liburing2 liburing-dev
fi

# Python deps preinstalled in ROCm image for prototyping
# Torch is preinstalled in ROCm image for prototyping

# Skipping DOCA?

# Force reinstall of RDMA packages from DOCA repository
# Reinstall needed to fix broken libibverbs-dev, which may lead to lack of Infiniband support.
# Upgrade is not sufficient if the version is the same since apt skips the installation.
# -- preinstalled in ROCm image for prototyping

# All other source deps preinstalled in ROCm image

# UCCL is skipped if no Nvidia GPU is present, but UCX may need to be.

# ROCm install prefix: prefer an explicit ROCM_PATH, else the value the image
# build recorded (the "new" layout installs to /opt/rocm/core-<MAJOR>.<MINOR>,
# not /opt/rocm), else the conventional /opt/rocm.
if [ -z "${ROCM_PATH}" ]; then
    if [ -f /etc/rocm-build.env ]; then
        # shellcheck disable=SC1091
        . /etc/rocm-build.env
        ROCM_PATH="${ROCM_INSTALL_PATH:-/opt/rocm}"
    else
        ROCM_PATH=/opt/rocm
    fi
fi

# Otherwise, lastly build and install nixl + nixlbench
if [ "${BUILD_NIXL_EP}" = "true" ]; then
    EXTRA_BUILD_ARGS="${EXTRA_BUILD_ARGS} -Dbuild_nixl_ep=true"
fi
# This image is CUDA-free, so restrict NIXL to plugins that build without cuFile
# (GDS/GDS_MT link -lcufile and would fail). POSIX alone is enough to produce
# libnixl for nixlbench to link.
# Override with NIXL_ENABLE_PLUGINS. build_docs=false: no doxygen in the image.
NIXL_ENABLE_PLUGINS="${NIXL_ENABLE_PLUGINS:-POSIX}"
# shellcheck disable=SC2086
meson setup ${NIXL_BUILD_DIR} --prefix=${INSTALL_DIR} -Ducx_path=${UCX_INSTALL_DIR} -Denable_plugins="${NIXL_ENABLE_PLUGINS}" -Dbuild_docs=false -Drust=false ${EXTRA_BUILD_ARGS} --buildtype=debug
ninja -j"$NPROC" -C ${NIXL_BUILD_DIR} && ninja -j"$NPROC" -C ${NIXL_BUILD_DIR} install

# TODO(kapila): Copy the nixl.pc file to the install directory if needed.
# cp ${BUILD_DIR}/nixl.pc ${INSTALL_DIR}/lib/pkgconfig/nixl.pc

cd benchmark/nixlbench
meson setup ${NIXLBENCH_BUILD_DIR} -Dnixl_path=${INSTALL_DIR} -Dprefix=${INSTALL_DIR} \
    -Duse_rocm=true -Drocm_path="${ROCM_PATH}"
ninja -j"$NPROC" -C ${NIXLBENCH_BUILD_DIR} && ninja -j"$NPROC" -C ${NIXLBENCH_BUILD_DIR} install