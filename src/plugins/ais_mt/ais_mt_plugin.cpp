/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "backend/backend_plugin.h"
#include "ais_mt_backend.h"

using ais_mt_plugin_t = nixlBackendPluginCreator<nixlAisMtEngine>;

#ifdef STATIC_PLUGIN_AIS_MT
nixlBackendPlugin *
createStaticAIS_MTPlugin() {
    return ais_mt_plugin_t::create(
        NIXL_PLUGIN_API_VERSION, "AIS_MT", "0.1.0", {}, {DRAM_SEG, VRAM_SEG, FILE_SEG});
}
#else
extern "C" NIXL_PLUGIN_EXPORT nixlBackendPlugin *
nixl_plugin_init() {
    return ais_mt_plugin_t::create(
        NIXL_PLUGIN_API_VERSION, "AIS_MT", "0.1.0", {}, {DRAM_SEG, VRAM_SEG, FILE_SEG});
}

extern "C" NIXL_PLUGIN_EXPORT void
nixl_plugin_fini() {}
#endif
