#!/bin/bash
#SBATCH --job-name=test-hipfile-ais-mt
#SBATCH --partition=defq
#SBATCH --gres=gpu:1
#SBATCH --cpus-per-task=8
#SBATCH --mem=32G
#SBATCH --time=02:00:00
#SBATCH --output=contrib/rocm/logs/test-hipfile-ais-mt-%j.log
#
# Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
# Build upstream NIXL and run AIS_MT validation on MARKHAM+NVME nodes.
# Submit from the nixl repo root:
#   sbatch --constraint='MARKHAM&NVME' contrib/rocm/test-hipfile-ais-mt.sbatch
#
set -euo pipefail

REPO_DIR="${SLURM_SUBMIT_DIR:-$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)}"
JOB_ROOT="/tmp/test-hipfile-ais-mt-${SLURM_JOB_ID:-local$$}"
NIXL_PREFIX="${JOB_ROOT}/nixl-install"
LOG_DIR="${REPO_DIR}/contrib/rocm/logs"
MICROSEMI_NVME="/dev/disk/by-id/nvme-MTR_SLC_16GB_0400000E3CBC"

mkdir -p "${JOB_ROOT}" "${LOG_DIR}"

export NIXL_ROCM_AIS_TEST_NVME="${NIXL_ROCM_AIS_TEST_NVME:-${NIXL_HIPFILE_TEST_NVME:-${MICROSEMI_NVME}}}"
export HIPFILE_ALLOW_COMPAT_MODE=false
export HIPFILE_UNSUPPORTED_FILE_SYSTEMS=true

echo "=== test-hipfile-ais-mt job ${SLURM_JOB_ID:-local} on $(hostname) ==="
echo "NIXL_ROCM_AIS_TEST_NVME=${NIXL_ROCM_AIS_TEST_NVME}"

if [[ ! -e "${NIXL_ROCM_AIS_TEST_NVME}" ]]; then
	echo "FAIL: Microsemi test NVMe not present: ${NIXL_ROCM_AIS_TEST_NVME}" >&2
	exit 1
fi
if [[ "${NIXL_ROCM_AIS_TEST_NVME}" != *"MTR_SLC_16GB"* ]]; then
	echo "FAIL: NVMe path must be the Microsemi MTR SLC test SSD by-id" >&2
	exit 1
fi

export NIXL_SRC="${REPO_DIR}"
export NIXL_INSTALL_PREFIX="${NIXL_PREFIX}"
export BUILD_UCX=1
"${REPO_DIR}/contrib/rocm/build-nixl-rocm.sh" 2>&1 | tee "${LOG_DIR}/build-${SLURM_JOB_ID:-local}.log"

export NIXL_PLUGIN_DIR="${NIXL_PREFIX}/lib/x86_64-linux-gnu/plugins"
if [[ ! -d "${NIXL_PLUGIN_DIR}" ]]; then
	NIXL_PLUGIN_DIR="${NIXL_PREFIX}/lib/nixl/plugins"
fi
export LD_LIBRARY_PATH="${NIXL_PREFIX}/lib/x86_64-linux-gnu:${NIXL_PREFIX}/lib:${LD_LIBRARY_PATH:-}"

TEST_BIN="${REPO_DIR}/build-rocm/test/unit/plugins/ais_mt/nixl_ais_mt_test"
if [[ ! -x "${TEST_BIN}" ]]; then
	echo "FAIL: ${TEST_BIN} not found" >&2
	exit 1
fi

echo "Running AIS_MT path-mode smoke..." | tee "${LOG_DIR}/unit-${SLURM_JOB_ID:-local}.log"
"${TEST_BIN}" 2>&1 | tee -a "${LOG_DIR}/unit-${SLURM_JOB_ID:-local}.log"

echo "=== test-hipfile-ais-mt PASS ==="
