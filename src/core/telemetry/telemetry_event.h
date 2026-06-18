/*
 * SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#ifndef NIXL_SRC_CORE_TELEMETRY_TELEMETRY_EVENT_H
#define NIXL_SRC_CORE_TELEMETRY_TELEMETRY_EVENT_H

#include <cstdint>
#include <string_view>

#include "nixl_types.h"

constexpr char TELEMETRY_BUFFER_SIZE_VAR[] = "NIXL_TELEMETRY_BUFFER_SIZE";
constexpr char TELEMETRY_RUN_INTERVAL_VAR[] = "NIXL_TELEMETRY_RUN_INTERVAL";

constexpr inline int TELEMETRY_VERSION = 4;

/** Sentinel gpu_id when a telemetry event is not associated with a GPU. */
constexpr int32_t NIXL_TELEMETRY_NO_GPU = -1;

/**
 * @enum nixl_telemetry_event_type_t
 * @brief Enumerates all known telemetry event types.
 */
enum class nixl_telemetry_event_type_t : uint32_t {
    AGENT_TX_BYTES = 0,
    AGENT_RX_BYTES = 1,
    AGENT_TX_REQUESTS_NUM = 2,
    AGENT_RX_REQUESTS_NUM = 3,
    AGENT_MEMORY_REGISTERED = 4,
    AGENT_MEMORY_DEREGISTERED = 5,
    AGENT_XFER_TIME = 6,
    AGENT_XFER_POST_TIME = 7,
    AGENT_ERR_NOT_POSTED = 8,
    AGENT_ERR_INVALID_PARAM = 9,
    AGENT_ERR_BACKEND = 10,
    AGENT_ERR_NOT_FOUND = 11,
    AGENT_ERR_MISMATCH = 12,
    AGENT_ERR_NOT_ALLOWED = 13,
    AGENT_ERR_REPOST_ACTIVE = 14,
    AGENT_ERR_UNKNOWN = 15,
    AGENT_ERR_NOT_SUPPORTED = 16,
    AGENT_ERR_REMOTE_DISCONNECT = 17,
    AGENT_ERR_CANCELED = 18,
    AGENT_ERR_NO_TELEMETRY = 19,
    AGENT_AIS_MT_READ_BYTES = 20,
    AGENT_AIS_MT_WRITE_BYTES = 21,
    AGENT_AIS_MT_READ_OPS = 22,
    AGENT_AIS_MT_WRITE_OPS = 23,
    AGENT_AIS_MT_READ_ERRORS = 24,
    AGENT_AIS_MT_WRITE_ERRORS = 25,
    AGENT_AIS_MT_SHORT_IO = 26,
    AGENT_AIS_MT_BUF_REGISTER_OK = 27,
    AGENT_AIS_MT_BUF_REGISTER_COMPAT = 28,
    AGENT_AIS_MT_BUF_REGISTER_ERRORS = 29,
    AGENT_AIS_MT_FILE_HANDLE_ERRORS = 30,
    AGENT_AIS_MT_HIP_DEVICE_ERRORS = 31,
    AGENT_AIS_MT_HIP_SYNC_ERRORS = 32,
    AGENT_AIS_MT_THREAD_COUNT = 33,
};

[[nodiscard]] nixl_telemetry_event_type_t
nixlTelemetryEventTypeForStatus(nixl_status_t s);

namespace nixlEnumStrings {
[[nodiscard]] constexpr std::string_view
telemetryEventTypeStr(const nixl_telemetry_event_type_t type) noexcept {
    switch (type) {
    case nixl_telemetry_event_type_t::AGENT_TX_BYTES:
        return "agent_tx_bytes";
    case nixl_telemetry_event_type_t::AGENT_RX_BYTES:
        return "agent_rx_bytes";
    case nixl_telemetry_event_type_t::AGENT_TX_REQUESTS_NUM:
        return "agent_tx_requests_num";
    case nixl_telemetry_event_type_t::AGENT_RX_REQUESTS_NUM:
        return "agent_rx_requests_num";
    case nixl_telemetry_event_type_t::AGENT_MEMORY_REGISTERED:
        return "agent_memory_registered";
    case nixl_telemetry_event_type_t::AGENT_MEMORY_DEREGISTERED:
        return "agent_memory_deregistered";
    case nixl_telemetry_event_type_t::AGENT_XFER_TIME:
        return "agent_xfer_time";
    case nixl_telemetry_event_type_t::AGENT_XFER_POST_TIME:
        return "agent_xfer_post_time";
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_POSTED:
        return "agent_err_not_posted";
    case nixl_telemetry_event_type_t::AGENT_ERR_INVALID_PARAM:
        return "agent_err_invalid_param";
    case nixl_telemetry_event_type_t::AGENT_ERR_BACKEND:
        return "agent_err_backend";
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_FOUND:
        return "agent_err_not_found";
    case nixl_telemetry_event_type_t::AGENT_ERR_MISMATCH:
        return "agent_err_mismatch";
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_ALLOWED:
        return "agent_err_not_allowed";
    case nixl_telemetry_event_type_t::AGENT_ERR_REPOST_ACTIVE:
        return "agent_err_repost_active";
    case nixl_telemetry_event_type_t::AGENT_ERR_UNKNOWN:
        return "agent_err_unknown";
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_SUPPORTED:
        return "agent_err_not_supported";
    case nixl_telemetry_event_type_t::AGENT_ERR_REMOTE_DISCONNECT:
        return "agent_err_remote_disconnect";
    case nixl_telemetry_event_type_t::AGENT_ERR_CANCELED:
        return "agent_err_canceled";
    case nixl_telemetry_event_type_t::AGENT_ERR_NO_TELEMETRY:
        return "agent_err_no_telemetry";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_BYTES:
        return "agent_ais_mt_read_bytes";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_BYTES:
        return "agent_ais_mt_write_bytes";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_OPS:
        return "agent_ais_mt_read_ops";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_OPS:
        return "agent_ais_mt_write_ops";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_ERRORS:
        return "agent_ais_mt_read_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_ERRORS:
        return "agent_ais_mt_write_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_SHORT_IO:
        return "agent_ais_mt_short_io";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_OK:
        return "agent_ais_mt_buf_register_ok";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_COMPAT:
        return "agent_ais_mt_buf_register_compat";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_ERRORS:
        return "agent_ais_mt_buf_register_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_FILE_HANDLE_ERRORS:
        return "agent_ais_mt_file_handle_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_DEVICE_ERRORS:
        return "agent_ais_mt_hip_device_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_SYNC_ERRORS:
        return "agent_ais_mt_hip_sync_errors";
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_THREAD_COUNT:
        return "agent_ais_mt_thread_count";
    }
    return "unknown_event";
}

[[nodiscard]] constexpr bool
telemetryEventUsesGpuLabel(const nixl_telemetry_event_type_t type) noexcept {
    switch (type) {
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_BYTES:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_BYTES:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_OPS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_OPS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_SHORT_IO:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_DEVICE_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_SYNC_ERRORS:
        return true;
    default:
        return false;
    }
}
}

/**
 * @struct nixlTelemetryEvent
 * @brief A structure to hold individual telemetry event data for cyclic buffer storage
 */
struct nixlTelemetryEvent {
    nixl_telemetry_event_type_t eventType_; // Detailed event type/identifier
    uint64_t value_; // Numeric value associated with the event
    int32_t gpuId_ = NIXL_TELEMETRY_NO_GPU; // GPU index for per-device metrics

    nixlTelemetryEvent() noexcept = default;

    nixlTelemetryEvent(nixl_telemetry_event_type_t event_type,
                       uint64_t value,
                       int32_t gpu_id = NIXL_TELEMETRY_NO_GPU) noexcept
        : eventType_(event_type),
          value_(value),
          gpuId_(gpu_id) {}
};

#endif
