/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <stdexcept>
#include <cstdlib>
#include <cstring>
#include <strings.h>
#include "common/nixl_log.h"
#include "hipfile_ais_mt_utils.h"

namespace {
bool
hipfileAisMtCompatModeAllowed() {
    const char *v = std::getenv("HIPFILE_ALLOW_COMPAT_MODE");
    if (v == nullptr || v[0] == '\0') {
        return false;
    }
    return std::strcmp(v, "1") == 0 || strcasecmp(v, "true") == 0 || strcasecmp(v, "yes") == 0;
}
} // namespace

hipfileAisMtUtil::hipfileAisMtUtil() {
    const hipFileError_t status = hipFileDriverOpen();
    if (status.err != hipFileSuccess) {
        throw std::runtime_error(
            "HIPFILE_AIS_MT: error initializing AMD Infinity Storage driver: error=" +
            std::to_string(status.err));
    }
}

hipfileAisMtUtil::~hipfileAisMtUtil() {
    (void)hipFileDriverClose();
}

hipfileAisMtMemBuf::hipfileAisMtMemBuf(void *ptr, size_t sz, int flags) : base_(ptr) {
    const hipFileError_t status = hipFileBufRegister(ptr, sz, flags);
    if (status.err != hipFileSuccess) {
        if (hipfileAisMtCompatModeAllowed()) {
            NIXL_WARN << "HIPFILE_AIS_MT: buffer registration failed - compat mode: err="
                      << status.err;
            return;
        }
        throw std::runtime_error(
            "HIPFILE_AIS_MT: hipFileBufRegister failed (err=" + std::to_string(status.err) +
            "); set HIPFILE_ALLOW_COMPAT_MODE=true to allow fallback");
    }
    registered_ = true;
}

hipfileAisMtMemBuf::~hipfileAisMtMemBuf() {
    if (registered_) {
        const hipFileError_t status = hipFileBufDeregister(base_);
        if (status.err != hipFileSuccess) {
            NIXL_WARN << "HIPFILE_AIS_MT: warning: deregistering buffer: error=" << status.err
                      << " ptr=" << base_;
        }
    }
}

hipfileAisMtFileHandle::hipfileAisMtFileHandle(int file_fd) : fd(file_fd) {
    hipFileDescr_t descr = {};
    descr.handle.fd = fd;
    descr.type = hipFileHandleTypeOpaqueFD;

    const hipFileError_t status = hipFileHandleRegister(&hip_fhandle, &descr);
    if (status.err != hipFileSuccess) {
        throw std::runtime_error("HIPFILE_AIS_MT: file register error: error=" +
                                 std::to_string(status.err) + ", fd=" + std::to_string(fd));
    }
}

hipfileAisMtFileHandle::~hipfileAisMtFileHandle() {
    (void)hipFileHandleDeregister(hip_fhandle);
}
