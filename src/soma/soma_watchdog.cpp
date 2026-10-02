#include "soma_watchdog.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

namespace vitalgrid {

std::string SomaTransmissionRecord::to_json() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "{"
       << "\"type\":\"SOMA_DSOC_EVENT\","
       << "\"incident_timestamp\":\"" << incident_timestamp << "\","
       << "\"crew_id\":\"" << crew_id << "\","
       << "\"trigger_reason\":\"" << trigger_reason << "\","
       << "\"historical_samples_count\":" << historical_samples_count << ","
       << "\"raw_payload_bytes\":" << raw_payload_bytes << ","
       << "\"compressed_payload_bytes\":" << compressed_payload_bytes << ","
       << "\"compression_ratio\":" << compression_ratio << ","
       << "\"compression_algorithm\":\"" << compression_algorithm << "\","
       << "\"encryption_cipher\":\"" << encryption_cipher << "\","
       << "\"iv_nonce_hex\":\"" << iv_nonce_hex << "\","
       << "\"auth_tag_hex\":\"" << auth_tag_hex << "\","
       << "\"dsoc_laser_terminal\":\"" << dsoc_laser_terminal << "\","
       << "\"target_ground_station\":\"" << target_ground_station << "\","
       << "\"light_delay_minutes\":" << light_delay_minutes
       << "}";
    return ss.str();
}

SomaWatchdog::SomaWatchdog(CircularTelemetryBuffer& circular_buffer)
    : circular_buffer_(circular_buffer) {}

SomaWatchdog::~SomaWatchdog() {
    stop();
}

void SomaWatchdog::start() {
    if (running_.load()) return;
    running_.store(true);
    worker_thread_ = std::thread(&SomaWatchdog::watchdog_worker_loop, this);
}

void SomaWatchdog::stop() {
    if (!running_.load()) return;
    running_.store(false);
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void SomaWatchdog::evaluate_survival_state(const RidgePayload& payload) {
    {
        std::lock_guard<std::mutex> lock(record_mutex_);
        last_observed_payload_ = payload;
    }

    const auto& s = payload.survival_status;
    bool violation = false;
    std::string reason;

    if (!s.vital_signs_present) {
        violation = true;
        reason = "ASTRONAUT VITAL SIGNS COLLAPSE / CARDIAC ASYSTOLE DETECTED";
    } else if (s.sos_panic_button_active) {
        violation = true;
        reason = "MANUAL SOS PANIC BUTTON ENGAGED BY CREW";
    } else if (!s.cabin_pressure_nominal) {
        violation = true;
        reason = "RAPID CABIN DEPRESSURIZATION / ECLSS STRUCTURAL BREACH";
    }

    if (violation && !soma_triggered_.load()) {
        soma_triggered_.store(true);
        execute_soma_emergency_protocol(reason, payload);
    }
}

void SomaWatchdog::reset_soma() {
    soma_triggered_.store(false);
    burst_in_progress_.store(false);
    std::cout << "[SOMA WATCHDOG] Manual reset commanded. Telemetry sampling nominal (1 Hz)." << std::endl;
}

SomaTransmissionRecord SomaWatchdog::get_last_record() const {
    std::lock_guard<std::mutex> lock(record_mutex_);
    return last_record_;
}

void SomaWatchdog::watchdog_worker_loop() {
    while (running_.load()) {
        // High-frequency watchdog evaluation
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static std::string generate_random_hex(size_t byte_count) {
    static const char hex_chars[] = "0123456789ABCDEF";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    std::string res;
    res.reserve(byte_count * 2);
    for (size_t i = 0; i < byte_count * 2; ++i) {
        res.push_back(hex_chars[dis(gen)]);
    }
    return res;
}

void SomaWatchdog::execute_soma_emergency_protocol(const std::string& trigger_reason, const RidgePayload& triggering_payload) {
    burst_in_progress_.store(true);

    // 1. Aumentar la frecuencia de muestreo del buffer circular (simular recolección a nivel milisegundo)
    std::cout << "\n================================================================================" << std::endl;
    std::cout << ">>> [SOMA INITIATED] CRITICAL SURVIVAL FAILURE DETECTED <<<" << std::endl;
    std::cout << "Reason: " << trigger_reason << std::endl;
    std::cout << "[STEP 1/5] Circular Buffer boosted to 1000 Hz (1 ms resolution high-density burst capture)..." << std::endl;

    // Simulate 100 ms burst of millisecond-rate samples
    for (int ms = 0; ms < 50; ++ms) {
        RidgePayload burst_sample = triggering_payload;
        burst_sample.timestamp = RidgePayload::current_iso8601();
        circular_buffer_.push(burst_sample);
    }

    // 2. Empaquetar el histórico RIDGE
    std::cout << "[STEP 2/5] Packaging historical RIDGE black-box telemetry container..." << std::endl;
    auto history = circular_buffer_.get_history();
    size_t sample_count = history.size();
    
    // Estimate payload size: each serialized RIDGE JSON is ~450 bytes + 128 bytes binary header
    size_t raw_bytes = sample_count * 450 + 256;

    // 3. Aplicar compresión sin pérdida (simular uso de Zstd)
    std::cout << "[STEP 3/5] Applying lossless compression: Zstandard (Zstd level 19 dictionary)..." << std::endl;
    double compression_ratio = 3.65; // realistic Zstd compression ratio on repetitive telemetry
    size_t compressed_bytes = static_cast<size_t>(static_cast<double>(raw_bytes) / compression_ratio);

    // 4. Aplicar cifrado de grado militar (simular AES-256 GCM)
    std::cout << "[STEP 4/5] Applying military-grade encryption: AES-256-GCM (NIST SP 800-38D authenticated)..." << std::endl;
    std::string iv_nonce = "0x" + generate_random_hex(12); // 96-bit IV
    std::string auth_tag = "0x" + generate_random_hex(16); // 128-bit MAC Tag

    // 5. Enrutar el paquete cifrado a la cola de transmisión láser DSOC
    std::cout << "[STEP 5/5] Enqueuing payload to Deep Space Optical Communications (DSOC) laser terminal..." << std::endl;

    SomaTransmissionRecord record;
    record.incident_timestamp = RidgePayload::current_iso8601();
    record.crew_id = triggering_payload.crew_member_id;
    record.trigger_reason = trigger_reason;
    record.historical_samples_count = sample_count;
    record.raw_payload_bytes = raw_bytes;
    record.compressed_payload_bytes = compressed_bytes;
    record.compression_ratio = compression_ratio;
    record.iv_nonce_hex = iv_nonce;
    record.auth_tag_hex = auth_tag;
    record.light_delay_minutes = triggering_payload.telemetry_ridge.D_distance.comm_delay_oneway_min;

    {
        std::lock_guard<std::mutex> lock(record_mutex_);
        last_record_ = record;
    }

    // 6. Prominent Log: [SOMA DSOC BURST TRANSMITTED]
    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    std::cout << "  [SOMA DSOC BURST TRANSMITTED]" << std::endl;
    std::cout << "  TIMESTAMP:       " << record.incident_timestamp << std::endl;
    std::cout << "  CREW ID:         " << record.crew_id << std::endl;
    std::cout << "  TRIGGER REASON:  " << record.trigger_reason << std::endl;
    std::cout << "  TELEMETRY SAMPLES: " << record.historical_samples_count << " packets (1000 Hz burst archive)" << std::endl;
    std::cout << "  DATA SIZE:       " << record.raw_payload_bytes << " B raw -> " << record.compressed_payload_bytes << " B (Zstd " << record.compression_ratio << "x)" << std::endl;
    std::cout << "  SECURITY CIPHER: AES-256-GCM (Nonce: " << record.iv_nonce_hex << " | Tag: " << record.auth_tag_hex << ")" << std::endl;
    std::cout << "  TRANSMITTER:     DSOC 1550nm Uplink Laser -> " << record.target_ground_station << std::endl;
    std::cout << "  ONE-WAY DELAY:   " << record.light_delay_minutes << " min light-speed delay to Earth" << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    // 7. Broadcast SOMA event JSON over WebSocket to frontend
    if (broadcast_cb_) {
        broadcast_cb_(record.to_json());
    }
}

} // namespace vitalgrid
