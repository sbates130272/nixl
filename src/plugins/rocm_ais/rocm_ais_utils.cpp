/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
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
#include "rocm_ais_utils.h"
#include "common/nixl_log.h"

nixl_status_t
rocmAisUtil::registerFileHandle(int fd,
                                size_t size,
                                std::string metaInfo,
                                rocmAisFileHandle &hip_handle) {
    hipFileError_t status;
    hipFileDescr_t descr = {};
    hipFileHandle_t handle;

    descr.handle.fd = fd;
    descr.type = hipFileHandleTypeOpaqueFD;

    status = hipFileHandleRegister(&handle, &descr);
    if (status.err != hipFileSuccess) {
        NIXL_ERROR << "hipFile register error: " << hipFileGetOpErrorString(status.err);
        return NIXL_ERR_BACKEND;
    }

    hip_handle.hip_fhandle = handle;
    hip_handle.fd = fd;
    hip_handle.size = size;
    hip_handle.metadata = metaInfo;

    return NIXL_SUCCESS;
}

nixl_status_t
rocmAisUtil::registerBufHandle(void *ptr, size_t size, int flags) {
    hipFileError_t status;

    status = hipFileBufRegister(ptr, size, flags);
    if (status.err != hipFileSuccess) {
        NIXL_WARN << "hipFile buffer registration failed - will use compat mode";
    }
    return NIXL_SUCCESS;
}

nixl_status_t
rocmAisUtil::openHipFileDriver() {
    hipFileError_t err;

    err = hipFileDriverOpen();
    if (err.err != hipFileSuccess) {
        NIXL_ERROR << "Error initializing hipFile AMD Infinity Storage driver: "
                   << hipFileGetOpErrorString(err.err);
        return NIXL_ERR_BACKEND;
    }
    return NIXL_SUCCESS;
}

void
rocmAisUtil::closeHipFileDriver() {
    (void)hipFileDriverClose();
}

void
rocmAisUtil::deregisterFileHandle(rocmAisFileHandle &handle) {
    (void)hipFileHandleDeregister(handle.hip_fhandle);
}

nixl_status_t
rocmAisUtil::deregisterBufHandle(void *ptr) {
    hipFileError_t status;

    status = hipFileBufDeregister(ptr);
    if (status.err != hipFileSuccess) {
        NIXL_ERROR << "Error de-registering buffer: " << hipFileGetOpErrorString(status.err);
        return NIXL_ERR_BACKEND;
    }
    return NIXL_SUCCESS;
}

nixlRocmAisIOBatch::nixlRocmAisIOBatch(unsigned int size) : max_reqs(size) {
    hipFileError_t err;

    io_batch_events = new hipFileIOEvents_t[size];
    io_batch_params = new hipFileIOParams_t[size];

    err = hipFileBatchIOSetUp(&batch_handle, size);
    if (err.err != hipFileSuccess) {
        NIXL_ERROR << "Error setting up hipFile batch: " << hipFileGetOpErrorString(err.err);
        init_err = err;
    }
}

nixlRocmAisIOBatch::~nixlRocmAisIOBatch() {
    if (current_status == NIXL_SUCCESS || current_status == NIXL_ERR_NOT_POSTED) {
        delete[] io_batch_events;
        delete[] io_batch_params;
        (void)hipFileBatchIODestroy(batch_handle);
    } else {
        NIXL_ERROR << "Attempting to delete a batch before completion";
    }
}

nixl_status_t
nixlRocmAisIOBatch::addToBatch(hipFileHandle_t fh,
                               void *buffer,
                               size_t size,
                               size_t file_offset,
                               size_t ptr_offset,
                               hipFileOpcode_t type) {
    hipFileIOParams_t *params = nullptr;

    if (batch_size >= max_reqs) {
        return NIXL_ERR_BACKEND;
    }

    params = &io_batch_params[batch_size];
    params->mode = hipFileBatch;
    params->fh = fh;
    params->u.batch.devPtr_base = buffer;
    params->u.batch.file_offset = file_offset;
    params->u.batch.devPtr_offset = ptr_offset;
    params->u.batch.size = size;
    params->opcode = type;
    params->cookie = params;
    batch_size++;

    return NIXL_SUCCESS;
}

nixl_status_t
nixlRocmAisIOBatch::cancelBatch() {
    hipFileError_t err;

    err = hipFileBatchIOCancel(batch_handle);
    if (err.err != hipFileSuccess) {
        NIXL_ERROR << "Error canceling hipFile batch: " << hipFileGetOpErrorString(err.err);
        return NIXL_ERR_BACKEND;
    }
    return NIXL_SUCCESS;
}

nixl_status_t
nixlRocmAisIOBatch::submitBatch(int flags) {
    hipFileError_t err;

    err = hipFileBatchIOSubmit(batch_handle, batch_size, io_batch_params, flags);
    if (err.err != hipFileSuccess) {
        NIXL_ERROR << "Error submitting hipFile batch: " << hipFileGetOpErrorString(err.err);
        return NIXL_ERR_BACKEND;
    }
    return NIXL_SUCCESS;
}

nixl_status_t
nixlRocmAisIOBatch::checkStatus() {
    hipFileError_t errBatch;
    unsigned int nr = batch_size;

    errBatch = hipFileBatchIOGetStatus(batch_handle, batch_size, &nr, io_batch_events, NULL);
    if (errBatch.err != hipFileSuccess) {
        NIXL_ERROR << "Error in hipFile batch get status: "
                   << hipFileGetOpErrorString(errBatch.err);
        current_status = NIXL_ERR_BACKEND;
    }

    entries_completed += nr;
    if (entries_completed < (unsigned int)batch_size) {
        current_status = NIXL_IN_PROG;
    } else if (entries_completed > batch_size) {
        current_status = NIXL_ERR_UNKNOWN;
    } else {
        current_status = NIXL_SUCCESS;
    }

    return current_status;
}

void
nixlRocmAisIOBatch::reset() {
    entries_completed = 0;
    batch_size = 0;
    current_status = NIXL_ERR_NOT_POSTED;
}
