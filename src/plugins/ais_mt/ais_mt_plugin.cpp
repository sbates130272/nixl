/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "backend/backend_plugin.h"
#include "hipfile_ais_mt_backend.h"
#include "common/nixl_log.h"
#include <exception>

using hipfile_ais_mt_plugin_t = nixlBackendPluginCreator<nixlHipfileAisMtEngine>;

#ifdef STATIC_PLUGIN_HIPFILE_AIS_MT
nixlBackendPlugin *
createStaticHIPFILE_AIS_MTPlugin() {
    return hipfile_ais_mt_plugin_t::create(
        NIXL_PLUGIN_API_VERSION, "HIPFILE_AIS_MT", "0.1.0", {}, {DRAM_SEG, VRAM_SEG, FILE_SEG});
}
#else
extern "C" NIXL_PLUGIN_EXPORT nixlBackendPlugin *
nixl_plugin_init() {
    return hipfile_ais_mt_plugin_t::create(
        NIXL_PLUGIN_API_VERSION, "HIPFILE_AIS_MT", "0.1.0", {}, {DRAM_SEG, VRAM_SEG, FILE_SEG});
}

extern "C" NIXL_PLUGIN_EXPORT void
nixl_plugin_fini() {}
#endif
