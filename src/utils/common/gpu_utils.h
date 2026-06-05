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

#ifndef NIXL_SRC_UTILS_COMMON_GPU_UTILS_H
#define NIXL_SRC_UTILS_COMMON_GPU_UTILS_H

#include <string>

namespace nixl {
namespace gpu {

    enum class Runtime { NONE, CUDA, ROCM };

    struct PointerInfo {
        bool is_device = false;
        int device_ordinal = -1;
        std::string pci_bus_id;
    };

    /** Return which GPU runtime was detected at build time. */
    [[nodiscard]] Runtime
    detected() noexcept;

    /**
     * Query attributes of a device pointer.
     * Returns non-zero on error.  When the pointer is host memory,
     * info.is_device is false and the other fields are unset.
     */
    [[nodiscard]] int
    queryPointer(void *addr, PointerInfo &info);

    /** Set the active GPU device for the calling thread. Returns non-zero on error. */
    [[nodiscard]] int
    setDevice(int ordinal);

    /** Return the number of visible GPUs, or 0 if no GPU runtime is present. */
    [[nodiscard]] int
    getDeviceCount() noexcept;

    /** Return the PCI bus ID string for the given device ordinal. */
    [[nodiscard]] std::string
    getPciBusId(int ordinal);

    /*
     * Opaque GPU context handle used by the libfabric backend for
     * context-pinning workarounds.  Each runtime backend stores its
     * own native type inside; callers interact only through the
     * functions below.
     */
    struct GpuCtx;

    /** Allocate and zero-initialise a GpuCtx. */
    [[nodiscard]] GpuCtx *
    gpuCtxCreate();

    /** Destroy a GpuCtx previously created with gpuCtxCreate(). */
    void
    gpuCtxDestroy(GpuCtx *ctx);

    /**
     * Update the context stored in @p ctx so that it matches the
     * device that owns @p address.  @p expected_dev is the ordinal
     * the caller expects.  @p was_updated is set to true when the
     * stored context was changed (first call for this device).
     * Returns non-zero on error.
     */
    [[nodiscard]] int
    gpuCtxUpdate(GpuCtx *ctx, void *address, int expected_dev, bool &was_updated);

    /** Make the context stored in @p ctx current on the calling thread. */
    [[nodiscard]] int
    gpuCtxApply(GpuCtx *ctx);

} // namespace gpu
} // namespace nixl

#endif // NIXL_SRC_UTILS_COMMON_GPU_UTILS_H
