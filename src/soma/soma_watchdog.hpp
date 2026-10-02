#pragma once

#include "circular_buffer.hpp"
#include "../models/ridge_payload.hpp"
#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace vitalgrid {

struct SomaTransmissionRecord {
    std::string incident_timestamp;
    std::string crew_id;
    std::string trigger_reason;
    size_t historical_samples_count = 0;
    size_t raw_payload_bytes = 0;
    size_t compressed_payload_bytes = 0;
    double compression_ratio = 1.0;
    std::string compression_algorithm = "Zstandard (v1.5.5 - level 19)";
    std::string encryption_cipher = "AES-256-GCM (NIST SP 800-38D)";
    std::string iv_nonce_hex;
    std::string auth_tag_hex;
    std::string dsoc_laser_terminal = "DSOC Flight Laser Transceiver (1550 nm, 267 Mbps)";
    std::string target_ground_station = "NASA DSN Goldstone 34m Beam Waveguide (Deep Space)";
    double light_delay_minutes = 20.5;

    std::string to_json() const;
};

/**
 * @brief High-priority Watchdog Thread: Spacecraft Offline Mortality & Anomaly Archive (SOMA)
 * Enforces "Science over Survival" emergency archival and autonomous deep-space laser burst transmission
 */
class SomaWatchdog {
public:
    using BroadcastCallback = std::function<void(const std::string&)>;

    explicit SomaWatchdog(CircularTelemetryBuffer& circular_buffer);
    ~SomaWatchdog();

    void start();
    void stop();

    /**
     * @brief Evaluates survival flags on every telemetry tick
     */
    void evaluate_survival_state(const RidgePayload& payload);

    /**
     * @brief Registers WebSocket broadcast hook for SOMA event propagation
     */
    void set_broadcast_callback(BroadcastCallback cb) {
        broadcast_cb_ = cb;
    }

    bool is_soma_triggered() const {
        return soma_triggered_.load();
    }

    /**
     * @brief Manually resets SOMA after clearance
     */
    void reset_soma();

    /**
     * @brief Returns latest transmission record
     */
    SomaTransmissionRecord get_last_record() const;

private:
    void watchdog_worker_loop();
    void execute_soma_emergency_protocol(const std::string& trigger_reason, const RidgePayload& triggering_payload);

    CircularTelemetryBuffer& circular_buffer_;
    std::atomic<bool> running_{false};
    std::atomic<bool> soma_triggered_{false};
    std::atomic<bool> burst_in_progress_{false};
    std::thread worker_thread_;

    mutable std::mutex record_mutex_;
    SomaTransmissionRecord last_record_;
    RidgePayload last_observed_payload_;
    BroadcastCallback broadcast_cb_;
};

} // namespace vitalgrid
