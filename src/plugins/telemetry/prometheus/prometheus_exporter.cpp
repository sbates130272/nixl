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
#include "prometheus_exporter.h"
#include "common/configuration.h"
#include "common/nixl_log.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <unordered_set>

namespace {
const uint16_t prometheusExporterDefaultPort = 9090;

const char prometheusPortVar[] = "NIXL_TELEMETRY_PROMETHEUS_PORT";
const char prometheusLocalVar[] = "NIXL_TELEMETRY_PROMETHEUS_LOCAL";

const std::string prometheusExporterLocalAddress = "127.0.0.1";
const std::string prometheusExporterPublicAddress = "0.0.0.0";

std::string
getHostname() {
    char hostname[HOST_NAME_MAX + 1];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        hostname[HOST_NAME_MAX] = '\0'; // Ensure null-termination
        return std::string(hostname);
    }
    return "unknown";
}

std::mutex s_mutex;
std::weak_ptr<prometheus::Exposer> s_exposer_weak;
std::weak_ptr<prometheus::Registry> s_registry_weak;
std::unordered_set<std::string> s_agent_names;

[[nodiscard]] bool
isErrorEvent(nixl_telemetry_event_type_t event_type) noexcept {
    switch (event_type) {
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_POSTED:
    case nixl_telemetry_event_type_t::AGENT_ERR_INVALID_PARAM:
    case nixl_telemetry_event_type_t::AGENT_ERR_BACKEND:
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_FOUND:
    case nixl_telemetry_event_type_t::AGENT_ERR_MISMATCH:
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_ALLOWED:
    case nixl_telemetry_event_type_t::AGENT_ERR_REPOST_ACTIVE:
    case nixl_telemetry_event_type_t::AGENT_ERR_UNKNOWN:
    case nixl_telemetry_event_type_t::AGENT_ERR_NOT_SUPPORTED:
    case nixl_telemetry_event_type_t::AGENT_ERR_REMOTE_DISCONNECT:
    case nixl_telemetry_event_type_t::AGENT_ERR_CANCELED:
    case nixl_telemetry_event_type_t::AGENT_ERR_NO_TELEMETRY:
        return true;
    default:
        return false;
    }
}

[[nodiscard]] bool
isAisMtCounterEvent(nixl_telemetry_event_type_t event_type) noexcept {
    switch (event_type) {
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_BYTES:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_BYTES:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_OPS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_OPS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_READ_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_WRITE_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_SHORT_IO:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_OK:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_COMPAT:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_BUF_REGISTER_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_FILE_HANDLE_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_DEVICE_ERRORS:
    case nixl_telemetry_event_type_t::AGENT_AIS_MT_HIP_SYNC_ERRORS:
        return true;
    default:
        return false;
    }
}
} // namespace

nixlTelemetryPrometheusExporter::nixlTelemetryPrometheusExporter(
    const nixlTelemetryExporterInitParams &init_params)
    : nixlTelemetryExporter(init_params),
      agent_name_(init_params.agentName),
      hostname_(getHostname()) {
    const bool local = nixl::config::getValueDefaulted(prometheusLocalVar, false);
    const uint16_t port =
        nixl::config::getValueDefaulted(prometheusPortVar, prometheusExporterDefaultPort);

    std::string bind_address;
    if (local) {
        bind_address = prometheusExporterLocalAddress + ":" + std::to_string(port);
    } else {
        bind_address = prometheusExporterPublicAddress + ":" + std::to_string(port);
    }

    const std::lock_guard lock(s_mutex);

    if (!s_agent_names.insert(agent_name_).second) {
        throw std::runtime_error("Prometheus exporter: duplicate agent name '" + agent_name_ +
                                 "'; each agent must have a unique name");
    }

    try {
        exposer_ = s_exposer_weak.lock();
        registry_ = s_registry_weak.lock();

        if (!exposer_ || !registry_) {
            registry_.reset();
            exposer_.reset();
            registry_ = std::make_shared<prometheus::Registry>();
            exposer_ = std::make_shared<prometheus::Exposer>(bind_address);
            exposer_->RegisterCollectable(registry_);
            s_exposer_weak = exposer_;
            s_registry_weak = registry_;
            NIXL_INFO << "Prometheus exporter initialized on " << bind_address;
        } else {
            NIXL_INFO << "Prometheus exporter for agent '" << agent_name_
                      << "' sharing existing server";
        }

        initializeMetrics();
    }
    catch (...) {
        counters_.clear();
        gauges_.clear();
        gpu_counter_families_.clear();
        gpu_counters_.clear();
        s_agent_names.erase(agent_name_);
        throw;
    }
}

nixlTelemetryPrometheusExporter::~nixlTelemetryPrometheusExporter() {
    const std::lock_guard lock(s_mutex);
    counters_.clear();
    gauges_.clear();
    gpu_counter_families_.clear();
    gpu_counters_.clear();
    s_agent_names.erase(agent_name_);
    exposer_.reset();
    registry_.reset();
}

// To make access cheaper we are creating static metrics with the labels already set
// Events are defined in the telemetry.cpp file
void
nixlTelemetryPrometheusExporter::initializeMetrics() {
    registerCounter("agent_tx_bytes", "Number of bytes sent by the agent");
    registerCounter("agent_rx_bytes", "Number of bytes received by the agent");
    registerCounter("agent_tx_requests_num", "Number of requests sent by the agent");
    registerCounter("agent_rx_requests_num", "Number of requests received by the agent");
    registerCounter("agent_memory_registered", "Cumulative memory registered");
    registerCounter("agent_memory_deregistered", "Cumulative memory deregistered");
    registerCounter("agent_xfer_time", "Start to Complete (per request)");
    registerCounter("agent_xfer_post_time", "Start to posting to Back-End (per request)");

    registerGauge("agent_memory_registered", "Memory registered");
    registerGauge("agent_memory_deregistered", "Memory deregistered");

    registerCounter("agent_err_not_posted", "Transfer not posted errors");
    registerCounter("agent_err_invalid_param", "Invalid parameter errors");
    registerCounter("agent_err_backend", "Backend errors");
    registerCounter("agent_err_not_found", "Not found errors");
    registerCounter("agent_err_mismatch", "Mismatch errors");
    registerCounter("agent_err_not_allowed", "Not allowed errors");
    registerCounter("agent_err_repost_active", "Repost while active errors");
    registerCounter("agent_err_unknown", "Unknown errors");
    registerCounter("agent_err_not_supported", "Not supported errors");
    registerCounter("agent_err_remote_disconnect", "Remote disconnect errors");
    registerCounter("agent_err_canceled", "Canceled transfer errors");
    registerCounter("agent_err_no_telemetry", "Telemetry unavailable errors");

    registerGpuCounterFamily("agent_ais_mt_read_bytes", "AIS_MT bytes read via hipFile");
    registerGpuCounterFamily("agent_ais_mt_write_bytes", "AIS_MT bytes written via hipFile");
    registerGpuCounterFamily("agent_ais_mt_read_ops", "AIS_MT hipFile read operations");
    registerGpuCounterFamily("agent_ais_mt_write_ops", "AIS_MT hipFile write operations");
    registerGpuCounterFamily("agent_ais_mt_read_errors", "AIS_MT hipFile read errors");
    registerGpuCounterFamily("agent_ais_mt_write_errors", "AIS_MT hipFile write errors");
    registerGpuCounterFamily("agent_ais_mt_short_io", "AIS_MT short read/write operations");
    registerGpuCounterFamily("agent_ais_mt_hip_device_errors", "AIS_MT hipSetDevice errors");
    registerGpuCounterFamily("agent_ais_mt_hip_sync_errors", "AIS_MT hipDeviceSynchronize errors");

    registerCounter("agent_ais_mt_buf_register_ok", "AIS_MT successful hipFile buffer registrations");
    registerCounter("agent_ais_mt_buf_register_compat",
                    "AIS_MT hipFile buffer registrations in compat mode");
    registerCounter("agent_ais_mt_buf_register_errors", "AIS_MT hipFile buffer registration errors");
    registerCounter("agent_ais_mt_file_handle_errors", "AIS_MT hipFile file handle errors");

    registerGauge("agent_ais_mt_thread_count", "AIS_MT Taskflow executor thread count");
}

void
nixlTelemetryPrometheusExporter::registerCounter(const std::string &name, const std::string &help) {
    auto &family = prometheus::BuildCounter().Name(name + "_total").Help(help).Register(*registry_);
    auto &metric = family.Add({{"hostname", hostname_}, {"agent_name", agent_name_}});
    const auto inserted = counters_.try_emplace(name, &family, &metric).second;
    if (!inserted) {
        family.Remove(&metric);
    }
    NIXL_ASSERT(inserted);
}

void
nixlTelemetryPrometheusExporter::registerGauge(const std::string &name, const std::string &help) {
    auto &family = prometheus::BuildGauge().Name(name).Help(help).Register(*registry_);
    auto &metric = family.Add({{"hostname", hostname_}, {"agent_name", agent_name_}});
    const auto inserted = gauges_.try_emplace(name, &family, &metric).second;
    if (!inserted) {
        family.Remove(&metric);
    }
    NIXL_ASSERT(inserted);
}

void
nixlTelemetryPrometheusExporter::registerGpuCounterFamily(const std::string &name,
                                                          const std::string &help) {
    auto &family = prometheus::BuildCounter().Name(name + "_total").Help(help).Register(*registry_);
    const auto inserted = gpu_counter_families_.try_emplace(name, &family).second;
    NIXL_ASSERT(inserted);
}

std::string
nixlTelemetryPrometheusExporter::gpuCounterKey(const std::string &name, int32_t gpu_id) {
    return name + ":" + std::to_string(gpu_id);
}

prometheus::Counter *
nixlTelemetryPrometheusExporter::getOrCreateGpuCounter(const std::string &name, int32_t gpu_id) {
    const std::string key = gpuCounterKey(name, gpu_id);
    const auto existing = gpu_counters_.find(key);
    if (existing != gpu_counters_.end()) {
        return existing->second.metric;
    }

    const auto family_it = gpu_counter_families_.find(name);
    NIXL_ASSERT(family_it != gpu_counter_families_.end());
    auto &family = *family_it->second;

    auto &metric = family.Add({{"hostname", hostname_},
                               {"agent_name", agent_name_},
                               {"gpu_id", std::to_string(gpu_id)}});
    const auto inserted = gpu_counters_.try_emplace(key, &family, &metric).second;
    if (!inserted) {
        family.Remove(&metric);
        return gpu_counters_.at(key).metric;
    }
    return &metric;
}

nixl_status_t
nixlTelemetryPrometheusExporter::exportEvent(const nixlTelemetryEvent &event) {
    // TODO(C++20): use std::string_view for lookup keys and transparent hash/equal_to
    // on counters_/gauges_ to avoid allocating a std::string per event when feasible.
    try {
        const std::string event_name(nixlEnumStrings::telemetryEventTypeStr(event.eventType_));

        if (nixlEnumStrings::telemetryEventUsesGpuLabel(event.eventType_)) {
            if (event.gpuId_ < 0) {
                NIXL_WARN << "Prometheus exporter: GPU-labeled event '" << event_name
                          << "' missing gpu_id; dropping";
                return NIXL_SUCCESS;
            }
            prometheus::Counter *counter = getOrCreateGpuCounter(event_name, event.gpuId_);
            counter->Increment(event.value_);
            return NIXL_SUCCESS;
        }

        if (isAisMtCounterEvent(event.eventType_) &&
            event.eventType_ != nixl_telemetry_event_type_t::AGENT_AIS_MT_THREAD_COUNT) {
            const auto counter = counters_.find(event_name);
            if (counter != counters_.end()) {
                counter->second.metric->Increment(event.value_);
            }
            return NIXL_SUCCESS;
        }

        const auto counter = counters_.find(event_name);
        if (counter != counters_.end()) {
            counter->second.metric->Increment(event.value_);
            return NIXL_SUCCESS;
        }

        const auto gauge = gauges_.find(event_name);
        if (gauge != gauges_.end()) {
            gauge->second.metric->Set(static_cast<double>(event.value_));
            return NIXL_SUCCESS;
        }

        if (isErrorEvent(event.eventType_)) {
            NIXL_WARN << "Prometheus exporter: unregistered error event '" << event_name << "'";
        }

        return NIXL_SUCCESS;
    }
    catch (const std::exception &e) {
        NIXL_ERROR << "Failed to export telemetry event: " << e.what();
        return NIXL_ERR_UNKNOWN;
    }
}
