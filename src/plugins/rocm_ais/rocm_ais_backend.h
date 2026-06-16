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

#ifndef __ROCM_AIS_BACKEND_H
#define __ROCM_AIS_BACKEND_H

#include <nixl.h>
#include <nixl_types.h>
#include <hip/hip_runtime.h>
#include <unistd.h>
#include <fcntl.h>
#include <list>
#include <vector>
#include <mutex>
#include <unordered_map>
#include "rocm_ais_utils.h"
#include "backend/backend_engine.h"

class nixlRocmAisMetadata : public nixlBackendMD {
public:
    rocmAisFileHandle handle;
    rocmAisMemBuf buf;
    nixl_mem_t type;

    nixlRocmAisMetadata() : nixlBackendMD(true) {}

    ~nixlRocmAisMetadata() {}
};

class RocmAisTransferRequestH {
public:
    void *addr;
    size_t size;
    size_t file_offset;
    hipFileHandle_t fh;
    hipFileOpcode_t op;

    RocmAisTransferRequestH() {
        addr = nullptr;
        size = 0;
        file_offset = 0;
        fh = nullptr;
        op = hipFileBatchRead;
    }

    RocmAisTransferRequestH(void *a,
                            size_t s,
                            size_t offset,
                            hipFileHandle_t handle,
                            hipFileOpcode_t operation) {
        addr = a;
        size = s;
        file_offset = offset;
        fh = handle;
        op = operation;
    }
};

class nixlRocmAisBackendReqH : public nixlBackendReqH {
public:
    std::vector<RocmAisTransferRequestH> request_list;
    std::vector<nixlRocmAisIOBatch *> batch_io_list;
    bool needs_prep;

    nixlRocmAisBackendReqH() {
        needs_prep = true;
    }

    ~nixlRocmAisBackendReqH() {
        for (auto *batch : batch_io_list) {
            delete batch;
        }
        batch_io_list.clear();
    }
};

class nixlRocmAisEngine : public nixlBackendEngine {
private:
    rocmAisUtil *rocm_ais_utils;
    std::unordered_map<int, rocmAisFileHandle> rocm_ais_file_map;

    mutable std::mutex batch_pool_lock;
    mutable std::list<nixlRocmAisIOBatch *> batch_pool;
    unsigned int batch_pool_size;
    unsigned int batch_limit;
    unsigned int max_request_size;

    nixlRocmAisIOBatch *
    getBatchFromPool(unsigned int size) const;
    void
    returnBatchToPool(nixlRocmAisIOBatch *batch) const;
    nixl_status_t
    createAndSubmitBatch(const std::vector<RocmAisTransferRequestH> &requests,
                         size_t start_idx,
                         size_t batch_size,
                         std::vector<nixlRocmAisIOBatch *> &batch_list) const;
    nixl_status_t
    createBatches(const nixl_xfer_op_t &operation,
                  const nixl_meta_dlist_t &local,
                  const nixl_meta_dlist_t &remote,
                  nixlRocmAisBackendReqH *handle);

public:
    nixlRocmAisEngine(const nixlBackendInitParams *init_params);
    ~nixlRocmAisEngine();

    bool
    supportsNotif() const {
        return false;
    }

    bool
    supportsRemote() const {
        return false;
    }

    bool
    supportsLocal() const {
        return true;
    }

    nixl_mem_list_t
    getSupportedMems() const {
        nixl_mem_list_t mems;
        mems.push_back(DRAM_SEG);
        mems.push_back(VRAM_SEG);
        mems.push_back(FILE_SEG);
        return mems;
    }

    nixl_status_t
    connect(const std::string &remote_agent) {
        return NIXL_SUCCESS;
    }

    nixl_status_t
    disconnect(const std::string &remote_agent) {
        return NIXL_SUCCESS;
    }

    nixl_status_t
    loadLocalMD(nixlBackendMD *input, nixlBackendMD *&output) {
        output = input;
        return NIXL_SUCCESS;
    }

    nixl_status_t
    unloadMD(nixlBackendMD *input) {
        return NIXL_SUCCESS;
    }

    nixl_status_t
    registerMem(const nixlBlobDesc &mem, const nixl_mem_t &nixl_mem, nixlBackendMD *&out);
    nixl_status_t
    deregisterMem(nixlBackendMD *meta);

    nixl_status_t
    prepXfer(const nixl_xfer_op_t &operation,
             const nixl_meta_dlist_t &local,
             const nixl_meta_dlist_t &remote,
             const std::string &remote_agent,
             nixlBackendReqH *&handle,
             const nixl_opt_b_args_t *opt_args = nullptr) const;

    nixl_status_t
    postXfer(const nixl_xfer_op_t &operation,
             const nixl_meta_dlist_t &local,
             const nixl_meta_dlist_t &remote,
             const std::string &remote_agent,
             nixlBackendReqH *&handle,
             const nixl_opt_b_args_t *opt_args = nullptr) const;

    nixl_status_t
    checkXfer(nixlBackendReqH *handle) const;
    nixl_status_t
    releaseReqH(nixlBackendReqH *handle) const;

    nixl_status_t
    queryMem(const nixl_reg_dlist_t &descs, std::vector<nixl_query_resp_t> &resp) const override;
};
#endif
