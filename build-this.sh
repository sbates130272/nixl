#!/bin/bash
# build-this.sh — build NIXL+nixlbench ROCm images and run nixlbench tests on WSL2/dxg.
#
# Viable backends on this node (WSL2, /dev/dxg, no /dev/kfd or IB):
#   UCX   — loopback via tcp+shared-mem transports (DRAM only; VRAM skipped, no kfd)
#   POSIX — AIO and io_uring variants (host dir bind-mounted at /mnt/nixlbench-posix)
#
# Multi-container UCX tests spin up a dedicated docker network + etcd, then run
# initiator and target as separate containers (pairwise pattern).
#
# Usage:
#   ./build-this.sh [--skip-build] [--posix-dir /path/on/host]

set -euo pipefail

POSIX_HOST_DIR="${POSIX_HOST_DIR:-/tmp/nixlbench-posix}"
SKIP_BUILD=0

for arg in "$@"; do
  case "$arg" in
    --skip-build) SKIP_BUILD=1 ;;
    --posix-dir) shift; POSIX_HOST_DIR="$1" ;;
  esac
done

# ---------------------------------------------------------------------------
# Step 1: Build the ROCm base OS image
# ---------------------------------------------------------------------------
if [ "$SKIP_BUILD" -eq 0 ]; then
  DOCKER_BUILDKIT=1 docker build \
    -f .ci/dockerfiles/Dockerfile.rocm.base \
    --secret id=amd_root_ca,src=AMDRootCA.crt \
    -t nixl-rocm-base:7.14.0 \
    .

  # ---------------------------------------------------------------------------
  # Step 2: Build the NIXL deps environment image (parallel BuildKit stages)
  # ---------------------------------------------------------------------------
  DOCKER_BUILDKIT=1 docker build \
    -f .ci/dockerfiles/Dockerfile.rocm \
    --build-arg ROCM_BASE_IMAGE=nixl-rocm-base:7.14.0 \
    --secret id=amd_root_ca,src=AMDRootCA.crt \
    --network=host \
    -t nixl-rocm-deps:local \
    .

  # ---------------------------------------------------------------------------
  # Step 3: Build NIXL and nixlbench inside a container, install to /opt/nixl
  # ---------------------------------------------------------------------------
  docker run --rm --user root --device /dev/dxg \
    -v "$(pwd)":/nixl-src:ro \
    -w /nixl-src \
    nixl-rocm-deps:local \
    bash -c ".gitlab/build-rocm.sh /opt/nixl"
fi

# ---------------------------------------------------------------------------
# Test helpers
# ---------------------------------------------------------------------------

NIXL_IMG="nixl-rocm-deps:local"
ETCD_IMG="quay.io/coreos/etcd:v3.5.18"
NIXLBENCH="/opt/nixl/bin/nixlbench-rocm"
ETCD_ENDPOINT="http://etcd:2379"

# Shared docker network for multi-container tests
NET="nixlbench-net"
docker network inspect "$NET" >/dev/null 2>&1 || docker network create "$NET"

run_etcd() {
  docker rm -f etcd 2>/dev/null || true
  docker run -d --name etcd --network "$NET" \
    -p 2379:2379 \
    "$ETCD_IMG" \
    /usr/local/bin/etcd \
      --data-dir=/etcd-data \
      --listen-client-urls=http://0.0.0.0:2379 \
      --advertise-client-urls=http://etcd:2379 \
      --listen-peer-urls=http://0.0.0.0:2380 \
      --initial-advertise-peer-urls=http://etcd:2380 \
      --initial-cluster=default=http://etcd:2380
  # Give etcd a moment to become ready
  sleep 2
}

stop_etcd() {
  docker rm -f etcd 2>/dev/null || true
}

nixl_run() {
  # nixl_run NAME [extra docker args...] -- [nixlbench args...]
  local name="$1"; shift
  local docker_args=()
  while [[ "$1" != "--" ]]; do docker_args+=("$1"); shift; done
  shift  # consume --
  docker run --rm --name "$name" \
    --network "$NET" \
    --device /dev/dxg \
    "${docker_args[@]}" \
    "$NIXL_IMG" \
    "$NIXLBENCH" "$@"
}

echo ""
echo "============================================================"
echo " nixlbench tests — WSL2/dxg node"
echo "============================================================"

# ---------------------------------------------------------------------------
# Test 1: UCX DRAM loopback — single container, both ranks in background
# ---------------------------------------------------------------------------
echo ""
echo "--- Test 1: UCX DRAM loopback (single container, two ranks) ---"
run_etcd
docker run --rm --device /dev/dxg --network "$NET" "$NIXL_IMG" bash -c "
  $NIXLBENCH --etcd_endpoints $ETCD_ENDPOINT \
    --backend UCX --initiator_seg_type DRAM --target_seg_type DRAM \
    --num_workers 1 &
  sleep 2
  $NIXLBENCH --etcd_endpoints $ETCD_ENDPOINT \
    --backend UCX --initiator_seg_type DRAM --target_seg_type DRAM \
    --num_workers 1
  wait
"
stop_etcd

# ---------------------------------------------------------------------------
# Test 2: UCX DRAM — two containers (initiator + target, pairwise)
# ---------------------------------------------------------------------------
echo ""
echo "--- Test 2: UCX DRAM pairwise (two containers) ---"
run_etcd
nixl_run nixlbench-target -- \
  --etcd_endpoints "$ETCD_ENDPOINT" \
  --backend UCX --initiator_seg_type DRAM --target_seg_type DRAM \
  --num_workers 1 &
TARGET_PID=$!
sleep 2
nixl_run nixlbench-initiator -- \
  --etcd_endpoints "$ETCD_ENDPOINT" \
  --backend UCX --initiator_seg_type DRAM --target_seg_type DRAM \
  --num_workers 1
wait "$TARGET_PID"
stop_etcd

# ---------------------------------------------------------------------------
# Test 3: POSIX AIO (host dir bind-mounted)
# ---------------------------------------------------------------------------
echo ""
echo "--- Test 3: POSIX AIO (host dir: $POSIX_HOST_DIR) ---"
mkdir -p "$POSIX_HOST_DIR"
docker run --rm --device /dev/dxg \
  -v "$POSIX_HOST_DIR":/mnt/nixlbench-posix \
  "$NIXL_IMG" \
  "$NIXLBENCH" \
    --backend POSIX \
    --filepath /mnt/nixlbench-posix/testfile \
    --posix_api_type AIO

# ---------------------------------------------------------------------------
# Test 4: POSIX io_uring (host dir bind-mounted)
# ---------------------------------------------------------------------------
echo ""
echo "--- Test 4: POSIX io_uring (host dir: $POSIX_HOST_DIR) ---"
docker run --rm --device /dev/dxg \
  -v "$POSIX_HOST_DIR":/mnt/nixlbench-posix \
  "$NIXL_IMG" \
  "$NIXLBENCH" \
    --backend POSIX \
    --filepath /mnt/nixlbench-posix/testfile \
    --posix_api_type URING

echo ""
echo "============================================================"
echo " All tests complete."
echo "============================================================"

# Cleanup shared network
docker network rm "$NET" 2>/dev/null || true
