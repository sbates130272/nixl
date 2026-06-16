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

# NIXL hipFile AIS Plugin

## Overview

> **Experimental:** hipFile batch I/O is not fully supported on the AMD
> backend yet. Prefer **HIPFILE_AIS_MT** for production workloads. Set
> `NIXL_HIPFILE_BATCH_TEST=1` to run full batch tests in
> `nixl_hipfile_ais_test`.

This plugin uses AMD hipFile APIs as an I/O backend for NIXL, enabling AMD
Infinity Storage on AMD GPUs. It parallels the NVIDIA GDS (cuFile) plugin—the
AIS path on ROCm—and supports transferring data between files and both system
memory (DRAM) and GPU memory (VRAM).

## Dependencies

- **ROCm** 7.1 or later with HIP runtime (hipFile requires ROCm 7.1+)
- **hipFile** library (from the [ROCm/hipFile][hipfile-repo] project or the
  rocs-ais package)
- A filesystem that supports AMD Infinity Storage (e.g., local NVMe, Lustre,
  GPFS, BeeGFS, or NFS with the appropriate GPU IO driver)

## Build Instructions

The plugin is built automatically when:

1. `HIPFILE_AIS` is in the enabled plugins list (default: all plugins enabled)
2. The HIP runtime is detected
3. The hipFile library (`libhipfile.so`) is found

### Meson options

| Option | Default | Description |
|--------|---------|-------------|
| `hipfile_ais_path` | auto-detect | Path to hipFile install prefix |
| `disable_hipfile_ais_backend` | `false` | Disable the hipFile AIS backend |

### Example commands

```bash
# Auto-detect ROCm and hipFile
meson setup builddir -Denable_plugins=HIPFILE_AIS

# Explicit paths
meson setup builddir \
  -Dhipfile_ais_path=/opt/rocs-ais

# Static plugin
meson setup builddir -Dstatic_plugins=HIPFILE_AIS
```

## API Reference

- **Backend name:** `HIPFILE_AIS` (pass to `nixlAgent::createBackend` or the C
  API equivalent).

### Backend parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `batch_pool_size` | 16 | Number of pre-allocated batch handles |
| `batch_limit` | 128 | Maximum requests per batch submission |
| `max_request_size` | 16 MB | Maximum bytes per individual I/O request |

### Supported memory types

| Memory Type | Description |
|-------------|-------------|
| `DRAM_SEG` | System (host) memory |
| `VRAM_SEG` | AMD GPU device memory |
| `FILE_SEG` | File descriptors on supported filesystems |

### Mutual exclusion

The `HIPFILE_AIS` plugin cannot be loaded simultaneously with the NVIDIA `GDS`
or `GDS_MT` plugins. This is enforced by the NIXL agent at backend creation
time.

## Example Usage

```cpp
nixl_b_params_t params;
params["batch_pool_size"] = "16";
params["batch_limit"] = "128";
params["max_request_size"] = "16777216"; // 16 MB

nixlBackendH *hipfile_ais;
agent.createBackend("HIPFILE_AIS", params, hipfile_ais);
```

<!-- References -->

[hipfile-repo]: https://github.com/ROCm/hipFile
