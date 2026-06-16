#!/bin/bash
# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
# Build NIXL with ROCm/HIP and hipFile from a source tree.
set -euo pipefail

NIXL_SRC="${NIXL_SRC:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
UCX_PREFIX="${UCX_PREFIX:-/opt/rocnixl-ucx}"
ROCM_PATH="${ROCM_PATH:-/opt/rocm}"
HIPFILE_PATH="${HIPFILE_PATH:-${ROCM_PATH}}"
NIXL_INSTALL_PREFIX="${NIXL_INSTALL_PREFIX:-/opt/nixl}"
BUILD_UCX="${BUILD_UCX:-0}"

if [[ ! -f "${NIXL_SRC}/meson.build" ]]; then
	echo "ERROR: ${NIXL_SRC}/meson.build not found" >&2
	exit 1
fi

export DEBIAN_FRONTEND=noninteractive
if command -v apt-get >/dev/null 2>&1; then
	sudo apt-get update
	sudo apt-get install -y --no-install-recommends \
		autoconf automake build-essential ca-certificates git \
		libaio-dev libibverbs-dev libltdl-dev libnuma-dev librdmacm-dev \
		libtool liburing-dev pkg-config rdma-core \
		|| true
fi

python3 -m pip install --no-cache-dir meson ninja pybind11 tomlkit

if [[ "${BUILD_UCX}" == "1" ]]; then
	mkdir -p /tmp/ucx-rocm
	cd /tmp/ucx-rocm
	if [[ ! -d ucx-src/.git ]]; then
		rm -rf ucx-src
		git clone --depth 1 https://github.com/ROCm/ucx.git -b v1.19.x ucx-src
	fi
	cd ucx-src
	./autogen.sh
	rm -rf build && mkdir build && cd build
	../configure \
		--prefix="${UCX_PREFIX}" \
		--enable-shared \
		--disable-static \
		--disable-doxygen-doc \
		--enable-optimizations \
		--enable-devel-headers \
		--with-rocm="${ROCM_PATH}" \
		--with-verbs \
		--with-dm \
		--enable-mt
	make -j"$(nproc)"
	sudo make install
	sudo ldconfig
fi

export PKG_CONFIG_PATH="${UCX_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="${UCX_PREFIX}/lib:${ROCM_PATH}/lib:${LD_LIBRARY_PATH:-}"

cd "${NIXL_SRC}"
rm -rf build-rocm
meson setup build-rocm \
	-Dwheel_variant=rocm \
	-Ducx_path="${UCX_PREFIX}" \
	-Drocm_ais_path="${HIPFILE_PATH}" \
	-Ddisable_gds_backend=true \
	"--prefix=${NIXL_INSTALL_PREFIX}"
meson compile -C build-rocm
sudo meson install -C build-rocm
sudo ldconfig

export NIXL_PLUGIN_DIR="${NIXL_INSTALL_PREFIX}/lib/x86_64-linux-gnu/plugins"
if [[ ! -d "${NIXL_PLUGIN_DIR}" ]]; then
	NIXL_PLUGIN_DIR="${NIXL_INSTALL_PREFIX}/lib/nixl/plugins"
fi
export PYTHONPATH="${NIXL_INSTALL_PREFIX}/lib/python3/dist-packages:${PYTHONPATH:-}"
export LD_LIBRARY_PATH="${NIXL_INSTALL_PREFIX}/lib/x86_64-linux-gnu:${NIXL_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}"

python3 -c "import nixl_rocm; print('nixl_rocm import OK')" 2>/dev/null \
	|| python3 -c "print('Python bindings optional if meson wheel not installed')"

for plug in AIS_MT ROCM_AIS; do
	found="$(find build-rocm -name "libplugin_${plug}.so" 2>/dev/null | head -1 || true)"
	if [[ -n "${found}" ]]; then
		echo "PASS: ${found}"
	else
		echo "WARN: libplugin_${plug}.so not found (hipFile/HIP may be missing)"
	fi
done

echo "NIXL ROCm build complete prefix=${NIXL_INSTALL_PREFIX}"
