# hipFile / AMD Infinity Storage — NIXL testing notes

Workstation and lab validation for the `ROCM_AIS` and `AIS_MT` plugins on
upstream NIXL. See also [`README.md`](README.md) for build and Slurm entry
points.

## Plugins

| Plugin | API | Status |
|--------|-----|--------|
| `AIS_MT` | Sync `hipFileRead` / `hipFileWrite` + Taskflow pool | **Production path** |
| `ROCM_AIS` | `hipFileBatchIO*` APIs | **Experimental** on AMD today |

Mutually exclusive with NVIDIA `GDS`, `GDS_MT`, and with each other (enforced
in `nixl_agent`).

## Build (workstation)

Debug builds include unit tests (`release` skips them).

```bash
cd ~/Projects/nixl
python3 -m venv .venv-build && source .venv-build/bin/activate
pip install meson ninja pybind11

meson setup build-rocm \
  -Dbuildtype=debug \
  -Ducx_path=$HOME/Projects/rocm-ucx/install \
  -Drocm_ais_path=/opt/rocm
meson compile -C build-rocm
```

Plugins appear under `build-rocm/src/plugins/` (`libplugin_ROCM_AIS*.so`,
`libplugin_AIS_MT*.so`).

## Runtime environment

```bash
export LD_LIBRARY_PATH=\
"build-rocm/src/utils/common:build-rocm/src/core:build-rocm/src/infra:\
build-rocm/src/plugins/ais_mt:build-rocm/src/plugins/rocm_ais:\
/opt/rocm/lib:${LD_LIBRARY_PATH:-}"

export HIPFILE_UNSUPPORTED_FILE_SYSTEMS=true
export NIXL_PLUGIN_DIR=build-rocm/src/plugins   # optional; silences warning
```

| Variable | Purpose |
|----------|---------|
| `HIPFILE_UNSUPPORTED_FILE_SYSTEMS=true` | Allow non-GDS filesystems in tests |
| `HIPFILE_ALLOW_COMPAT_MODE=true` | Allow registration fallback (DRAM only) |
| `HIPFILE_ALLOW_COMPAT_MODE=false` | Fail if true AIS path unavailable (Slurm) |
| `NIXL_ROCM_AIS_TEST_NVME` | Path-mode smoke target (default: Microsemi by-id) |
| `NIXL_ROCM_AIS_BATCH_TEST=1` | Enable full storage test in `nixl_rocm_ais_test` |

## Test hardware

All destructive NVMe I/O must use the disposable Microsemi MTR SLC SSD by
stable identity (never `/dev/nvmeXnY`):

`/dev/disk/by-id/nvme-MTR_SLC_16GB_0400000E3CBC`

Workstation layout used for validation:

- SSD formatted and mounted at `/mnt/microsemi`
- Test directory: `/mnt/microsemi/nixl-test`
- GPU: AMD RX 9070 XT (ROCm), hipFile from `/opt/rocm`

Path-mode smoke must not open the raw block device with `fopen()`. Point
`NIXL_ROCM_AIS_TEST_NVME` at a file on the mount, for example
`/mnt/microsemi/nixl-test/path-smoke.bin`.

## Unit test binaries

| Binary | Role |
|--------|------|
| `build-rocm/test/unit/plugins/ais_mt/nixl_ais_mt_test` | Primary hardware test |
| `build-rocm/test/unit/plugins/rocm_ais/nixl_rocm_ais_test` | Path smoke default; batch test gated |
| `build-rocm/test/nixl/test_plugin build-rocm/src/plugins` | Plugin load smoke |

### `nixl_ais_mt_test` flags

- `-P` — skip path-mode smoke (no `-P` on `nixl_rocm_ais_test`; that binary has
  no such flag)
- `-v` / `-d` — VRAM (default) or DRAM
- `-n N` — number of parallel transfer descriptors (use `-n 1` today)
- `-s SIZE` — per-transfer size (`1M`, `64M`, …)
- `-D` — `O_DIRECT` on test files
- `-N N` — Taskflow `thread_count` backend parameter

## Validation results (snoc-thinkstation, 2026-06-05)

### Passed — `AIS_MT` VRAM

All runs used `-P -v -n 1` and directory `/mnt/microsemi/nixl-test`. Phase 5
reported **Verification completed successfully!**

| Case | Size | Notes | Write / Read |
|------|------|-------|--------------|
| Baseline | 1 MiB | cached | ~1.0 / ~1.5 GB/s |
| O_DIRECT | 4 MiB | `-D` | ~1.3 / ~1.4 GB/s |
| Large I/O | 64 MiB | cached | ~4.4 / ~4.3 GB/s |
| Thread pool | 4 MiB | `-N 8` | ~1.3 / ~1.4 GB/s |

Interpretation: VRAM ↔ files on the Microsemi mount over hipFile AIS is
working end-to-end on the production MT backend, including direct I/O and
Taskflow threading for single-descriptor requests.

### Failed or not supported — expected on current ROCm hipFile

**`AIS_MT` DRAM** (`-d`, `HIPFILE_ALLOW_COMPAT_MODE=true`):

- `hipFileBufRegister` returns **5013** (`hipFileHipMemoryTypeInvalid`)
- Compat mode logs a warning and skips registration; `hipFileWrite` still
  fails (return value encodes hipFile error; log may show `strerror(0)` as
  `"Success"` — misleading)
- Host DRAM AIS is not a supported validation target on this stack today

**`ROCM_AIS` batch plugin** (`NIXL_ROCM_AIS_BATCH_TEST=1`):

- `hipFileBatchIOSubmit`: **Internal GPU IO library error** (VRAM and DRAM)
- Documented as experimental until AMD batch backend is complete

**Multi-descriptor requests** (`-n 2` or default `-n 250`):

- VRAM write/read can succeed but Phase 5 validation fails on buffer ≥ 1
- Use **`-n 1`** for hardware smoke until batch descriptor handling is fixed

**Path-mode smoke** (default first step when no directory arg):

- Works when `NIXL_ROCM_AIS_TEST_NVME` is a regular file on the test mount
- Fails or is unsafe when set to the raw by-id block device

### Plugin manager warning (harmless in debug builds)

`Error accessing directory("build-rocm/src/core/plugins")` — debug builds load
plugins via `build-rocm/pluginlist`. Set `NIXL_PLUGIN_DIR` as above or ignore.

## Recommended smoke command

```bash
export TESTDIR=/mnt/microsemi/nixl-test
mkdir -p "$TESTDIR"

./build-rocm/test/unit/plugins/ais_mt/nixl_ais_mt_test \
  -P -v -n 1 -s 1M "$TESTDIR"
```

## Slurm (MARKHAM + NVME)

From repo root:

```bash
sbatch --constraint='MARKHAM&NVME' contrib/rocm/test-hipfile-ais-mt.sbatch
```

Uses `HIPFILE_ALLOW_COMPAT_MODE=false` and the Microsemi by-id path. Runs
path-mode smoke only (no directory argument in the sbatch script).

## Code fixes included in this branch (post-enablement)

- **getopt:** optstring `…DPhN…` so `-P` is not parsed as requiring an
  argument (was forcing VRAM when user passed `-P -d`)
- **MT backend:** `hipSetDevice` per request, sequential ops in one Taskflow
  task, `hipDeviceSynchronize` in `checkXfer`
- **Test:** `hipDeviceSynchronize` before validation `hipMemcpy`; `fsync` after
  write phase (multi-transfer investigation)

## Open items

1. Root-cause and fix multi-descriptor validation (`-n ≥ 2`)
2. Improve MT error logging for negative `hipFileRead`/`hipFileWrite` returns
   (use `hipFileGetOpErrorString`, not `strerror(errno)`)
3. Path-mode smoke: refuse block devices or default to a file under `$TESTDIR`
4. DRAM AIS when ROCm hipFile supports host buffers on target hardware
5. `ROCM_AIS` batch when AMD ships batch backend support
6. Cluster rerun via `contrib/rocm/test-hipfile-ais-mt.sbatch`

## Sign-off bar (Tier-2 bare metal)

**Pass:** `AIS_MT` VRAM, `-n 1`, mounted Microsemi test directory, write + read
+ pattern verify.

**Not required for enablement sign-off:** DRAM AIS, batch plugin full test,
default 250-transfer workload, Slurm (until MARKHAM+NVME available).
