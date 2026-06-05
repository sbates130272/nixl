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
#include <cassert>
#include <hipfile.h>
#include "hipfile_ais_backend.h"
#include "hipfile_ais_utils.h"
#include "common/nixl_log.h"
#include "file/file_utils.h"
#include <unordered_map>
#include <memory>
#include <hip/hip_runtime.h>

#define DEFAULT_BATCH_LIMIT 128
#define DEFAULT_MAX_REQUEST_SIZE (16 * 1024 * 1024) // 16MB
#define DEFAULT_BATCH_POOL_SIZE 16

nixlHipfileAisEngine::nixlHipfileAisEngine(const nixlBackendInitParams *init_params)
    : nixlBackendEngine(init_params) {
    hipfile_utils = new hipfileUtil();

    batch_pool_size = DEFAULT_BATCH_POOL_SIZE;
    batch_limit = DEFAULT_BATCH_LIMIT;
    max_request_size = DEFAULT_MAX_REQUEST_SIZE;

    nixl_b_params_t *custom_params = init_params->customParams;
    if (custom_params) {
        if (custom_params->count("batch_pool_size") > 0) {
            try {
                batch_pool_size = std::stoi((*custom_params)["batch_pool_size"]);
            }
            catch (const std::exception &e) {
                NIXL_ERROR << "Invalid batch_pool_size parameter: " << e.what();
                this->initErr = true;
                return;
            }
        }

        if (custom_params->count("batch_limit") > 0) {
            try {
                batch_limit = std::stoi((*custom_params)["batch_limit"]);
            }
            catch (const std::exception &e) {
                NIXL_ERROR << "Invalid batch_limit parameter: " << e.what();
                this->initErr = true;
                return;
            }
        }

        if (custom_params->count("max_request_size") > 0) {
            try {
                max_request_size = std::stoul((*custom_params)["max_request_size"]);
            }
            catch (const std::exception &e) {
                NIXL_ERROR << "Invalid max_request_size parameter: " << e.what();
                this->initErr = true;
                return;
            }
        }
    }

    this->initErr = false;
    if (hipfile_utils->openHipFileDriver() == NIXL_ERR_BACKEND) {
        this->initErr = true;
        return;
    }

    for (unsigned int i = 0; i < batch_pool_size; i++) {
        batch_pool.push_back(new nixlHipfileIOBatch(batch_limit));
    }
}

nixl_status_t
nixlHipfileAisEngine::registerMem(const nixlBlobDesc &mem,
                                  const nixl_mem_t &nixl_mem,
                                  nixlBackendMD *&out) {
    nixl_status_t status = NIXL_SUCCESS;
    nixlHipfileAisMetadata *md = new nixlHipfileAisMetadata();
    md->type = nixl_mem;
    hipError_t error_id;

    switch (nixl_mem) {
    case FILE_SEG: {
        auto it = hipfile_file_map.find(mem.devId);
        if (it != hipfile_file_map.end()) {
            md->handle = it->second;
            md->handle.size = mem.len;
            md->handle.metadata = mem.metaInfo;
            break;
        }

        status = hipfile_utils->registerFileHandle(mem.devId, mem.len, mem.metaInfo, md->handle);
        if (status == NIXL_SUCCESS) {
            hipfile_file_map[mem.devId] = md->handle;
        }
        break;
    }

    case VRAM_SEG: {
        error_id = hipSetDevice(mem.devId);
        if (error_id != hipSuccess) {
            NIXL_ERROR << "hipSetDevice returned " << hipGetErrorString(error_id)
                       << " for device ID " << mem.devId;
            delete md;
            return NIXL_ERR_BACKEND;
        }
        status = hipfile_utils->registerBufHandle((void *)mem.addr, mem.len, 0);
        if (status == NIXL_SUCCESS) {
            md->buf.base = (void *)mem.addr;
            md->buf.size = mem.len;
        }
        break;
    }

    case DRAM_SEG: {
        status = hipfile_utils->registerBufHandle((void *)mem.addr, mem.len, 0);
        if (status == NIXL_SUCCESS) {
            md->buf.base = (void *)mem.addr;
            md->buf.size = mem.len;
        }
        break;
    }

    default:
        status = NIXL_ERR_BACKEND;
        break;
    }

    if (status != NIXL_SUCCESS) {
        delete md;
        return status;
    }

    out = (nixlBackendMD *)md;
    return status;
}

nixl_status_t
nixlHipfileAisEngine::deregisterMem(nixlBackendMD *meta) {
    nixlHipfileAisMetadata *md = (nixlHipfileAisMetadata *)meta;
    if (md->type == FILE_SEG) {
        hipfile_utils->deregisterFileHandle(md->handle);
        hipfile_file_map.erase(md->handle.fd);
    } else {
        hipfile_utils->deregisterBufHandle(md->buf.base);
    }
    delete md;
    return NIXL_SUCCESS;
}

nixl_status_t
nixlHipfileAisEngine::prepXfer(const nixl_xfer_op_t &operation,
                               const nixl_meta_dlist_t &local,
                               const nixl_meta_dlist_t &remote,
                               const std::string &remote_agent,
                               nixlBackendReqH *&handle,
                               const nixl_opt_b_args_t *opt_args) const {
    nixlHipfileAisBackendReqH *hf_handle = new nixlHipfileAisBackendReqH();
    size_t buf_cnt = local.descCount();
    size_t file_cnt = remote.descCount();

    if ((buf_cnt != file_cnt) || ((operation != NIXL_READ) && (operation != NIXL_WRITE))) {
        NIXL_ERROR << "Error in count or operation selection";
        delete hf_handle;
        return NIXL_ERR_INVALID_PARAM;
    }

    if ((remote.getType() != FILE_SEG) && (local.getType() != FILE_SEG)) {
        NIXL_ERROR << "Only support I/O between memory (DRAM/VRAM) and file type";
        delete hf_handle;
        return NIXL_ERR_INVALID_PARAM;
    }

    hf_handle->request_list.clear();

    bool is_local_file = (local.getType() == FILE_SEG);

    for (size_t i = 0; i < buf_cnt; i++) {
        void *base_addr;
        size_t total_size;
        size_t base_offset;
        hipfileFileHandle fh;

        if (is_local_file) {
            base_addr = (void *)remote[i].addr;
            if (!base_addr) {
                delete hf_handle;
                return NIXL_ERR_INVALID_PARAM;
            }
            total_size = remote[i].len;
            base_offset = (size_t)local[i].addr;

            auto it = hipfile_file_map.find(local[i].devId);
            if (it == hipfile_file_map.end()) {
                NIXL_ERROR << "File handle not found";
                delete hf_handle;
                return NIXL_ERR_NOT_FOUND;
            }
            fh = it->second;
        } else {
            base_addr = (void *)local[i].addr;
            if (!base_addr) {
                delete hf_handle;
                return NIXL_ERR_INVALID_PARAM;
            }
            total_size = local[i].len;
            base_offset = (size_t)remote[i].addr;

            auto it = hipfile_file_map.find(remote[i].devId);
            if (it == hipfile_file_map.end()) {
                NIXL_ERROR << "File handle not found";
                delete hf_handle;
                return NIXL_ERR_NOT_FOUND;
            }
            fh = it->second;
        }

        size_t remaining_size = total_size;
        size_t current_offset = 0;

        while (remaining_size > 0) {
            size_t request_size = std::min(remaining_size, (size_t)max_request_size);

            HipfileTransferRequestH req;
            req.addr = (char *)base_addr + current_offset;
            req.size = request_size;
            req.file_offset = base_offset + current_offset;
            req.fh = fh.hip_fhandle;
            req.op = (operation == NIXL_READ) ? hipFileBatchRead : hipFileBatchWrite;

            hf_handle->request_list.push_back(req);

            remaining_size -= request_size;
            current_offset += request_size;
        }
    }

    if (hf_handle->request_list.empty()) {
        delete hf_handle;
        return NIXL_ERR_INVALID_PARAM;
    }

    hf_handle->needs_prep = false;
    handle = hf_handle;
    return NIXL_SUCCESS;
}

nixlHipfileIOBatch *
nixlHipfileAisEngine::getBatchFromPool(unsigned int size) const {
    const std::lock_guard<std::mutex> lock(batch_pool_lock);
    if (!batch_pool.empty()) {
        nixlHipfileIOBatch *batch = batch_pool.back();
        batch_pool.pop_back();
        batch->reset();
        return batch;
    }
    return nullptr;
}

void
nixlHipfileAisEngine::returnBatchToPool(nixlHipfileIOBatch *batch) const {
    const std::lock_guard<std::mutex> lock(batch_pool_lock);
    batch_pool.push_back(batch);
}

nixl_status_t
nixlHipfileAisEngine::postXfer(const nixl_xfer_op_t &operation,
                               const nixl_meta_dlist_t &local,
                               const nixl_meta_dlist_t &remote,
                               const std::string &remote_agent,
                               nixlBackendReqH *&handle,
                               const nixl_opt_b_args_t *opt_args) const {
    nixlHipfileAisBackendReqH *hf_handle = (nixlHipfileAisBackendReqH *)handle;

    if (hf_handle->request_list.empty()) {
        NIXL_ERROR << "Empty request list";
        return NIXL_ERR_INVALID_PARAM;
    }

    const auto &request_list = hf_handle->request_list;
    size_t current_req = 0;

    while (current_req < request_list.size()) {
        size_t this_batch_size = std::min(request_list.size() - current_req, (size_t)batch_limit);
        nixl_status_t status = createAndSubmitBatch(
            request_list, current_req, this_batch_size, hf_handle->batch_io_list);

        if (status != NIXL_SUCCESS) {
            for (auto *batch : hf_handle->batch_io_list) {
                batch->cancelBatch();
                returnBatchToPool(batch);
            }
            hf_handle->batch_io_list.clear();
            return status;
        }
        current_req += this_batch_size;
    }

    return NIXL_IN_PROG;
}

nixl_status_t
nixlHipfileAisEngine::createAndSubmitBatch(const std::vector<HipfileTransferRequestH> &requests,
                                           size_t start_idx,
                                           size_t batch_size,
                                           std::vector<nixlHipfileIOBatch *> &batch_list) const {
    nixlHipfileIOBatch *batch = getBatchFromPool(batch_size);
    if (!batch) {
        NIXL_ERROR << "hipFile batch pool exhausted";
        return NIXL_ERR_BACKEND;
    }

    for (size_t i = 0; i < batch_size; i++) {
        const auto &req = requests[start_idx + i];
        if (!req.addr || !req.fh) {
            returnBatchToPool(batch);
            return NIXL_ERR_INVALID_PARAM;
        }

        nixl_status_t status =
            batch->addToBatch(req.fh, req.addr, req.size, req.file_offset, 0, req.op);
        if (status != NIXL_SUCCESS) {
            returnBatchToPool(batch);
            return NIXL_ERR_INVALID_PARAM;
        }
    }

    nixl_status_t status = batch->submitBatch(0);
    if (status != NIXL_SUCCESS) {
        returnBatchToPool(batch);
        return NIXL_ERR_BACKEND;
    }

    batch_list.push_back(batch);
    return NIXL_SUCCESS;
}

nixl_status_t
nixlHipfileAisEngine::checkXfer(nixlBackendReqH *handle) const {
    nixlHipfileAisBackendReqH *hf_handle = (nixlHipfileAisBackendReqH *)handle;

    if (hf_handle->batch_io_list.empty()) {
        hf_handle->needs_prep = true;
        return NIXL_SUCCESS;
    }

    nixl_status_t status = NIXL_SUCCESS;
    for (auto *batch : hf_handle->batch_io_list) {
        status = batch->checkStatus();

        if (status == NIXL_IN_PROG) {
            return status;
        }

        if (status < 0) {
            batch->cancelBatch();
        }
        returnBatchToPool(batch);
    }

    hf_handle->batch_io_list.clear();
    hf_handle->needs_prep = true;
    return status;
}

nixl_status_t
nixlHipfileAisEngine::releaseReqH(nixlBackendReqH *handle) const {
    nixlHipfileAisBackendReqH *hf_handle = (nixlHipfileAisBackendReqH *)handle;

    delete hf_handle;

    return NIXL_SUCCESS;
}

nixlHipfileAisEngine::~nixlHipfileAisEngine() {
    for (auto *batch : batch_pool) {
        if (batch) {
            delete batch;
        }
    }
    batch_pool.clear();

    if (hipfile_utils) {
        hipfile_utils->closeHipFileDriver();
        delete hipfile_utils;
    }
}

nixl_status_t
nixlHipfileAisEngine::queryMem(const nixl_reg_dlist_t &descs,
                               std::vector<nixl_query_resp_t> &resp) const {
    std::vector<nixl_blob_t> metadata(descs.descCount());
    for (int i = 0; i < descs.descCount(); ++i) {
        metadata[i] = descs[i].metaInfo;
    }

    return nixl::queryFileInfoList(metadata, resp);
}
