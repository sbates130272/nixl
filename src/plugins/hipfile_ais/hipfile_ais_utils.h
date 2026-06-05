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
#ifndef __HIPFILE_AIS_UTILS_H
#define __HIPFILE_AIS_UTILS_H

#include <fcntl.h>
#include <unistd.h>
#include <nixl.h>
#include <hipfile.h>

class hipfileFileHandle {
public:
    int fd;
    size_t size;
    std::string metadata;
    hipFileHandle_t hip_fhandle;
};

class hipfileMemBuf {
public:
    void *base;
    size_t size;
};

class nixlHipfileIOBatch {
public:
    nixlHipfileIOBatch(unsigned int size);
    ~nixlHipfileIOBatch();

    nixl_status_t
    addToBatch(hipFileHandle_t fh,
               void *buffer,
               size_t size,
               size_t file_offset,
               size_t ptr_offset,
               hipFileOpcode_t type);
    nixl_status_t
    submitBatch(int flags);
    nixl_status_t
    checkStatus();
    nixl_status_t
    cancelBatch();
    void
    reset();

private:
    hipFileBatchHandle_t batch_handle;
    hipFileIOEvents_t *io_batch_events = nullptr;
    hipFileIOParams_t *io_batch_params = nullptr;
    hipFileError_t init_err = {hipFileSuccess};
    unsigned int max_reqs = 0;
    unsigned int batch_size = 0;
    unsigned int entries_completed = 0;
    nixl_status_t current_status = NIXL_ERR_NOT_POSTED;
};

class hipfileUtil {
public:
    hipfileUtil() {}

    ~hipfileUtil() {}

    nixl_status_t
    registerFileHandle(int fd, size_t size, std::string metaInfo, hipfileFileHandle &handle);
    nixl_status_t
    registerBufHandle(void *ptr, size_t size, int flags);
    void
    deregisterFileHandle(hipfileFileHandle &handle);
    nixl_status_t
    deregisterBufHandle(void *ptr);
    nixl_status_t
    openHipFileDriver();
    void
    closeHipFileDriver();
};
#endif
