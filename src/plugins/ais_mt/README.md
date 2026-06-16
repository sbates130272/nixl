<!--
Copyright (c) Advanced Micro Devices, Inc. All rights reserved.

SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# NIXL AIS_MT Plugin (hipFile multi-threaded)

## Overview

Production AMD Infinity Storage backend for NIXL. Uses synchronous
`hipFileRead` / `hipFileWrite` with a Taskflow thread pool—the supported path on
current hipFile AMD backends. Prefer this backend over `ROCM_AIS` until hipFile
batch I/O is fully supported on AMD.

## Dependencies

- **ROCm** 7.1 or later with HIP runtime (hipFile requires ROCm 7.1+)
- hipFile (`libhipfile.so`) from [ROCm/rocm-systems][rocm-systems]
- Taskflow (bundled via Meson subproject, same pattern as the NVIDIA `GDS_MT`
  plugin)
- For hardware tests: Microsemi MTR SLC test SSD only:
  `/dev/disk/by-id/nvme-MTR_SLC_16GB_0400000E3CBC`

## Build Instructions

```bash
meson setup build -Dwheel_variant=rocm \
  -Ducx_path=/opt/rocnixl-ucx \
  -Drocm_ais_path=/opt/rocm
meson compile -C build
```

Or use [`contrib/rocm/build-nixl-rocm.sh`](../../../contrib/rocm/build-nixl-rocm.sh).

## API Reference

- **Backend name:** `AIS_MT` (pass to `nixlAgent::createBackend` or the C API
  equivalent).

### Backend parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `thread_count` | `max(1, hardware_concurrency()/2)` | Taskflow worker threads for sync I/O |

### Environment variables

| Variable | Purpose |
|----------|---------|
| `HIPFILE_ALLOW_COMPAT_MODE=false` | Fail if true AIS path unavailable (recommended) |
| `HIPFILE_UNSUPPORTED_FILE_SYSTEMS=true` | Allow test filesystems when needed |
| `NIXL_ROCM_AIS_TEST_NVME` | Override test NVMe path (default: Microsemi by-id above) |

### Mutual exclusion

Cannot load simultaneously with `GDS`, `GDS_MT`, or `ROCM_AIS`.

### Related backends

- Batch variant: `ROCM_AIS` (experimental until hipFile batch AMD backend ships)
- NVIDIA equivalent: `GDS_MT`

## Example Usage

```cpp
nixl_b_params_t params;
params["thread_count"] = "8";
nixlBackendH *ais_mt;
agent.createBackend("AIS_MT", params, ais_mt);
```

<!-- References -->

[rocm-systems]: https://github.com/ROCm/rocm-systems/tree/develop/projects/hipfile
