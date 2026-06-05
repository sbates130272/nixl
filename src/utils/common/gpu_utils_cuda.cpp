/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <cuda.h>
#include <cuda_runtime.h>

namespace nixl {
namespace gpu {

    struct GpuCtx {
        CUcontext cu_ctx = nullptr;
        int dev_id = -1;
    };

    Runtime
    detected() noexcept {
        return Runtime::CUDA;
    }

    int
    queryPointer(void *addr, PointerInfo &info) {
        CUmemorytype mem_type = CU_MEMORYTYPE_HOST;
        uint32_t is_managed = 0;
        CUdevice dev = 0;
        CUcontext ctx = nullptr;

        CUpointer_attribute attr_type[4];
        void *attr_data[4];

        attr_type[0] = CU_POINTER_ATTRIBUTE_MEMORY_TYPE;
        attr_data[0] = &mem_type;
        attr_type[1] = CU_POINTER_ATTRIBUTE_IS_MANAGED;
        attr_data[1] = &is_managed;
        attr_type[2] = CU_POINTER_ATTRIBUTE_DEVICE_ORDINAL;
        attr_data[2] = &dev;
        attr_type[3] = CU_POINTER_ATTRIBUTE_CONTEXT;
        attr_data[3] = &ctx;

        CUresult result =
            cuPointerGetAttributes(4, attr_type, attr_data, reinterpret_cast<CUdeviceptr>(addr));
        if (result != CUDA_SUCCESS) {
            const char *err_str = nullptr;
            cuGetErrorString(result, &err_str);
            NIXL_ERROR << "cuPointerGetAttributes failed: " << (err_str ? err_str : "unknown");
            return -1;
        }

        info.is_device = (mem_type == CU_MEMORYTYPE_DEVICE);
        info.device_ordinal = static_cast<int>(dev);
        info.pci_bus_id.clear();

        if (info.is_device) {
            char pci_buf[32];
            CUresult pci_result = cuDeviceGetPCIBusId(pci_buf, sizeof(pci_buf), dev);
            if (pci_result == CUDA_SUCCESS) {
                info.pci_bus_id = pci_buf;
            }
        }

        return 0;
    }

    int
    setDevice(int ordinal) {
        cudaError_t err = cudaSetDevice(ordinal);
        if (err != cudaSuccess) {
            NIXL_ERROR << "cudaSetDevice(" << ordinal << ") failed: " << cudaGetErrorString(err);
            return -1;
        }
        return 0;
    }

    int
    getDeviceCount() noexcept {
        int count = 0;
        cudaError_t err = cudaGetDeviceCount(&count);
        if (err != cudaSuccess) {
            return 0;
        }
        return count;
    }

    std::string
    getPciBusId(int ordinal) {
        char pci_buf[32];
        CUresult result =
            cuDeviceGetPCIBusId(pci_buf, sizeof(pci_buf), static_cast<CUdevice>(ordinal));
        if (result != CUDA_SUCCESS) {
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

        CUcontext cur_ctx = nullptr;
        CUpointer_attribute attr = CU_POINTER_ATTRIBUTE_CONTEXT;
        void *data = &cur_ctx;
        CUresult result =
            cuPointerGetAttributes(1, &attr, &data, reinterpret_cast<CUdeviceptr>(address));
        if (result != CUDA_SUCCESS) {
            return -1;
        }

        if (ctx->cu_ctx) {
            if (ctx->cu_ctx != cur_ctx) {
                return -1;
            }
            return 0;
        }

        ctx->cu_ctx = cur_ctx;
        ctx->dev_id = expected_dev;
        was_updated = true;
        return 0;
    }

    int
    gpuCtxApply(GpuCtx *ctx) {
        if (!ctx || ctx->cu_ctx == nullptr) {
            return 0;
        }
        CUresult result = cuCtxSetCurrent(ctx->cu_ctx);
        return (result == CUDA_SUCCESS) ? 0 : -1;
    }

} // namespace gpu
} // namespace nixl
