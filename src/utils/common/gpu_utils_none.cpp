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

namespace nixl {
namespace gpu {

    struct GpuCtx {};

    Runtime
    detected() noexcept {
        return Runtime::NONE;
    }

    int
    queryPointer(void * /*addr*/, PointerInfo &info) {
        info = {};
        return -1;
    }

    int
    setDevice(int /*ordinal*/) {
        return -1;
    }

    int
    getDeviceCount() noexcept {
        return 0;
    }

    std::string
    getPciBusId(int /*ordinal*/) {
        return {};
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
    gpuCtxUpdate(GpuCtx * /*ctx*/, void * /*address*/, int /*expected_dev*/, bool &was_updated) {
        was_updated = false;
        return -1;
    }

    int
    gpuCtxApply(GpuCtx * /*ctx*/) {
        return 0;
    }

} // namespace gpu
} // namespace nixl
