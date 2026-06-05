/* Copyright (c) Advanced Micro Devices, Inc. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef __HIPFILE_AIS_MT_UTILS_H
#define __HIPFILE_AIS_MT_UTILS_H

#include <fcntl.h>
#include <unistd.h>
#include <nixl.h>
#include <hipfile.h>

class hipfileAisMtFileHandle {
public:
    hipfileAisMtFileHandle(int fd);
    ~hipfileAisMtFileHandle();

    hipfileAisMtFileHandle(const hipfileAisMtFileHandle &) = delete;
    hipfileAisMtFileHandle &
    operator=(const hipfileAisMtFileHandle &) = delete;
    hipfileAisMtFileHandle(hipfileAisMtFileHandle &&) = delete;
    hipfileAisMtFileHandle &
    operator=(hipfileAisMtFileHandle &&) = delete;

    int fd{-1};
    hipFileHandle_t hip_fhandle{nullptr};
};

class hipfileAisMtMemBuf {
public:
    hipfileAisMtMemBuf(void *ptr, size_t sz, int flags = 0);
    ~hipfileAisMtMemBuf();

    hipfileAisMtMemBuf(const hipfileAisMtMemBuf &) = delete;
    hipfileAisMtMemBuf &
    operator=(const hipfileAisMtMemBuf &) = delete;
    hipfileAisMtMemBuf(hipfileAisMtMemBuf &&) = delete;
    hipfileAisMtMemBuf &
    operator=(hipfileAisMtMemBuf &&) = delete;

private:
    void *base_{nullptr};
    bool registered_{false};
};

class hipfileAisMtUtil {
public:
    hipfileAisMtUtil();
    ~hipfileAisMtUtil();
};
#endif
