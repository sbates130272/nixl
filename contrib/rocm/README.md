# ROCm / hipFile enablement

Build and test helpers for AMD Infinity Storage (hipFile) in upstream NIXL.

Hardware validation notes and workstation test results:
[`README-hipfile.md`](README-hipfile.md).

## Build

```bash
./contrib/rocm/build-nixl-rocm.sh
```

Set `BUILD_UCX=1` to compile ROCm UCX 1.19.x first. Requires hipFile under
`/opt/rocm` or `-Drocm_ais_path=...`.

## Hardware tests (Microsemi NVMe only)

All NVMe I/O tests must use the disposable Microsemi MTR SLC SSD:

`/dev/disk/by-id/nvme-MTR_SLC_16GB_0400000E3CBC`

```bash
export NIXL_ROCM_AIS_TEST_NVME=/dev/disk/by-id/nvme-MTR_SLC_16GB_0400000E3CBC
export HIPFILE_ALLOW_COMPAT_MODE=false
export HIPFILE_UNSUPPORTED_FILE_SYSTEMS=true
./build-debug/test/unit/plugins/ais_mt/nixl_ais_mt_test
```

Slurm (MARKHAM+NVME):

```bash
sbatch --constraint='MARKHAM&NVME' contrib/rocm/test-hipfile-ais-mt.sbatch
```

## CI

- Compile-only: `.ci/dockerfiles/Dockerfile.rocm-build`
- Jenkins matrix: `.ci/jenkins/lib/build-rocm-matrix.yaml`
