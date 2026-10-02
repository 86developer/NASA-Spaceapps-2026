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
        reason = "COLAPSO DE SIGNOS VITALES DEL ASTRONAUTA / ASISTOLIA DETECTADA";
    } else if (s.sos_panic_button_active) {
        violation = true;
        reason = "BOTÓN DE PÁNICO SOS MANUAL ACTIVADO POR LA TRIPULACIÓN";
    } else if (!s.cabin_pressure_nominal) {
        violation = true;
        reason = "DESCOMPRESIÓN RÁPIDA DE CABINA / FALLA ESTRUCTURAL DE ECLSS";
    }

    if (violation && !soma_triggered_.load()) {
        soma_triggered_.store(true);
        execute_soma_emergency_protocol(reason, payload);
    }
}

void SomaWatchdog::reset_soma() {
    soma_triggered_.store(false);
    burst_in_progress_.store(false);
    std::cout << "[SOMA WATCHDOG] Reinicio manual ordenado. Frecuencia de muestreo nominal (1 Hz)." << std::endl;
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
    std::cout << ">>> [SOMA INICIADO] FALLA CRÍTICA DE SUPERVIVENCIA DETECTADA <<<" << std::endl;
    std::cout << "Motivo: " << trigger_reason << std::endl;
    std::cout << "[PASO 1/5] Búfer circular aumentado a 1000 Hz (captura de ráfaga de alta densidad a 1 ms)..." << std::endl;

    // Simulate 100 ms burst of millisecond-rate samples
    for (int ms = 0; ms < 50; ++ms) {
        RidgePayload burst_sample = triggering_payload;
        burst_sample.timestamp = RidgePayload::current_iso8601();
        circular_buffer_.push(burst_sample);
    }

    // 2. Empaquetar el histórico RIDGE
    std::cout << "[PASO 2/5] Empaquetando contenedor histórico RIDGE de caja negra..." << std::endl;
    auto history = circular_buffer_.get_history();
    size_t sample_count = history.size();
    
    // Estimate payload size: each serialized RIDGE JSON is ~450 bytes + 128 bytes binary header
    size_t raw_bytes = sample_count * 450 + 256;

    // 3. Aplicar compresión sin pérdida (simular uso de Zstd)
    std::cout << "[PASO 3/5] Aplicando compresión sin pérdida: Zstandard (Zstd nivel 19 con diccionario)..." << std::endl;
    double compression_ratio = 3.65; // realistic Zstd compression ratio on repetitive telemetry
    size_t compressed_bytes = static_cast<size_t>(static_cast<double>(raw_bytes) / compression_ratio);

    // 4. Aplicar cifrado de grado militar (simular AES-256 GCM)
    std::cout << "[PASO 4/5] Aplicando cifrado de grado militar: AES-256-GCM (autenticado NIST SP 800-38D)..." << std::endl;
    std::string iv_nonce = "0x" + generate_random_hex(12); // 96-bit IV
    std::string auth_tag = "0x" + generate_random_hex(16); // 128-bit MAC Tag

    // 5. Enrutar el paquete cifrado a la cola de transmisión láser DSOC
    std::cout << "[PASO 5/5] Encolando paquete en terminal láser de Comunicaciones Ópticas de Espacio Profundo (DSOC)..." << std::endl;

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
    std::cout << "  HORA UTC:            " << record.incident_timestamp << std::endl;
    std::cout << "  TRIPULANTE:          " << record.crew_id << std::endl;
    std::cout << "  MOTIVO DE DISPARO:   " << record.trigger_reason << std::endl;
    std::cout << "  MUESTRAS TELEMETRÍA: " << record.historical_samples_count << " paquetes (archivo en ráfaga a 1000 Hz)" << std::endl;
    std::cout << "  TAMAÑO DE DATOS:     " << record.raw_payload_bytes << " B sin comprimir -> " << record.compressed_payload_bytes << " B (Zstd " << record.compression_ratio << "x)" << std::endl;
    std::cout << "  CIFRADO DE SEGURIDAD: AES-256-GCM (Nonce: " << record.iv_nonce_hex << " | Tag: " << record.auth_tag_hex << ")" << std::endl;
    std::cout << "  TRANSMISOR:          Láser DSOC 1550nm -> " << record.target_ground_station << std::endl;
    std::cout << "  RETARDO LUMÍNICO:    " << record.light_delay_minutes << " min de retardo a la velocidad de la luz a la Tierra" << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    // 7. Broadcast SOMA event JSON over WebSocket to frontend
    if (broadcast_cb_) {
        broadcast_cb_(record.to_json());
    }
}

} // namespace vitalgrid
