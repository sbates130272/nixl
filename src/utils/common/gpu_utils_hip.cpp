/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 Advanced Micro Devices, Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gpu_utils.h"
#include "nixl_log.h"

#include <hip/hip_runtime.h>

namespace nixl {
namespace gpu {

    struct GpuCtx {
        int dev_id = -1;
    };

    Runtime
    detected() noexcept {
        return Runtime::ROCM;
    }

    int
    queryPointer(void *addr, PointerInfo &info) {
        hipPointerAttribute_t attribs = {};
        hipError_t err = hipPointerGetAttributes(&attribs, addr);
        if (err != hipSuccess) {
            NIXL_ERROR << "hipPointerGetAttributes failed: " << hipGetErrorString(err);
            return -1;
        }

#if HIP_VERSION >= 60000000
        info.is_device = (attribs.type == hipMemoryTypeDevice);
#else
        info.is_device = (attribs.memoryType == hipMemoryTypeDevice);
#endif
        info.device_ordinal = attribs.device;
        info.pci_bus_id.clear();

        if (info.is_device) {
            char pci_buf[32];
            hipError_t pci_err =
                hipDeviceGetPCIBusId(pci_buf, sizeof(pci_buf), info.device_ordinal);
            if (pci_err == hipSuccess) {
                info.pci_bus_id = pci_buf;
            }
        }

        return 0;
    }

    int
    setDevice(int ordinal) {
        hipError_t err = hipSetDevice(ordinal);
        if (err != hipSuccess) {
            NIXL_ERROR << "hipSetDevice(" << ordinal << ") failed: " << hipGetErrorString(err);
            return -1;
        }
        return 0;
    }

    int
    getDeviceCount() noexcept {
        int count = 0;
        hipError_t err = hipGetDeviceCount(&count);
        if (err != hipSuccess) {
            return 0;
        }
        return count;
    }

    std::string
    getPciBusId(int ordinal) {
        char pci_buf[32];
        hipError_t err = hipDeviceGetPCIBusId(pci_buf, sizeof(pci_buf), ordinal);
        if (err != hipSuccess) {
            return {};
        }
        return {pci_buf};
    }

    GpuCtx *
    gpuCtxCreate() {
        return new GpuCtx();
    }

    void
    gpuCtxDestroy(GpuCtx *ctx) {
        delete ctx;
    }

    int
    gpuCtxUpdate(GpuCtx *ctx, void *address, int expected_dev, bool &was_updated) {
        was_updated = false;
        if (!ctx || expected_dev == -1) {
            return -1;
        }
        if (ctx->dev_id != -1 && expected_dev != ctx->dev_id) {
            return -1;
        }

        PointerInfo pinfo;
        int ret = queryPointer(address, pinfo);
        if (ret) {
            return ret;
        }
        if (!pinfo.is_device) {
            return 0;
        }
        if (pinfo.device_ordinal != expected_dev) {
            return -1;
        }

        if (ctx->dev_id != -1) {
            return 0;
        }

        ctx->dev_id = expected_dev;
        was_updated = true;
        return 0;
    }

    int
    gpuCtxApply(GpuCtx *ctx) {
        if (!ctx || ctx->dev_id == -1) {
            return 0;
        }
        return setDevice(ctx->dev_id);
    }

} // namespace gpu
} // namespace nixl
