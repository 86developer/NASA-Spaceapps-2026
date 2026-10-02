#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>
#include <csignal>
#include <random>
#include <algorithm>

#include "models/ridge_payload.hpp"
#include "models/alert_payload.hpp"
#include "engine/flight_surgeon.hpp"
#include "soma/circular_buffer.hpp"
#include "soma/soma_watchdog.hpp"
#include "network/websocket_server.hpp"

using namespace vitalgrid;

static std::atomic<bool> g_keep_running{true};

void signal_handler(int signum) {
    std::cout << "\n[VITALGRID] Termination signal (" << signum << ") received. Initiating graceful shutdown..." << std::endl;
    g_keep_running.store(false);
}

int main(int argc, char* argv[]) {
    int port = 9002;
    if (argc > 1) {
        try {
            port = std::stoi(argv[1]);
        } catch (...) {
            std::cerr << "Invalid port argument. Using default: 9002\n";
        }
    }

    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "================================================================================" << std::endl;
    std::cout << "  NASA SPACE APPS CHALLENGE - DEEP SPACE ASTRONAUT HEALTH MONITORING" << std::endl;
    std::cout << "  VITALGRID FLIGHT SURGEON AI & SOMA ARCHIVAL DAEMON [v1.0-AEROSPACE-RELEASE]" << std::endl;
    std::cout << "  Target Architecture: Deep Space Transit (Earth-Mars Cycler / Gateway / Artemis)" << std::endl;
    std::cout << "================================================================================\n" << std::endl;

    // 1. Initialize High-Rate Telemetry Ring Buffer
    CircularTelemetryBuffer telemetry_buffer(2048);

    // 2. Initialize Flight Surgeon AI
    FlightSurgeonAI flight_surgeon;

    // 3. Initialize SOMA Watchdog Thread
    SomaWatchdog soma_watchdog(telemetry_buffer);

    // 4. Initialize WebSocket Server
    WebSocketServer ws_server(port);

    // Wire SOMA broadcast to WebSocket
    soma_watchdog.set_broadcast_callback([&ws_server](const std::string& soma_json) {
        ws_server.broadcast(soma_json);
    });

    if (!ws_server.start()) {
        std::cerr << "[CRITICAL ERROR] Failed to start WebSocket daemon on port " << port << std::endl;
        return 1;
    }

    soma_watchdog.start();

    // Baseline RIDGE Telemetry State
    std::mutex state_mutex;
    RidgePayload state;
    state.timestamp = RidgePayload::current_iso8601();
    state.crew_member_id = "CMD-01";
    state.health_score = 92;
    state.survival_status.vital_signs_present = true;
    state.survival_status.sos_panic_button_active = false;
    state.survival_status.cabin_pressure_nominal = true;

    // Base RIDGE metrics (from user prompt reference)
    state.telemetry_ridge.R_radiation.career_accumulated_mSv = 145.2;
    state.telemetry_ridge.R_radiation.acute_event_mSv = 12.0;

    state.telemetry_ridge.I_isolation.sleep_hours_last_24h = 5.8;
    state.telemetry_ridge.I_isolation.hrv_rmssd_drop_pct = 18.5;
    state.telemetry_ridge.I_isolation.pvt_reaction_time_ms = 280;

    state.telemetry_ridge.D_distance.comm_delay_oneway_min = 20.5;
    state.telemetry_ridge.D_distance.med_supply_days_remaining = 412;

    state.telemetry_ridge.G_gravity.heart_rate_bpm = 105;
    state.telemetry_ridge.G_gravity.blood_pressure_sys = 135;
    state.telemetry_ridge.G_gravity.blood_pressure_dia = 85;
    state.telemetry_ridge.G_gravity.onsd_mm = 5.2;
    state.telemetry_ridge.G_gravity.exercise_completed_hours = 1.5;

    state.telemetry_ridge.E_environment.cabin_ppco2_mmHg = 3.2;
    state.telemetry_ridge.E_environment.spo2_pct = 94;
    state.telemetry_ridge.E_environment.body_temperature_c = 37.9;

    // Remote Command Handling from UI
    ws_server.set_message_callback([&](socket_t /*client*/, const std::string& msg) {
        std::lock_guard<std::mutex> lock(state_mutex);
        std::cout << "[REMOTE INGESTION COMMAND] Received: " << msg << std::endl;

        if (msg == "TRIGGER_SOS") {
            state.survival_status.sos_panic_button_active = true;
        } else if (msg == "DEPRESSURIZE") {
            state.survival_status.cabin_pressure_nominal = false;
        } else if (msg == "ASYSTOLE") {
            state.survival_status.vital_signs_present = false;
            state.telemetry_ridge.G_gravity.heart_rate_bpm = 0;
            state.telemetry_ridge.G_gravity.blood_pressure_sys = 0;
            state.telemetry_ridge.G_gravity.blood_pressure_dia = 0;
            state.telemetry_ridge.E_environment.spo2_pct = 0;
        } else if (msg == "RESET_SURVIVAL") {
            state.survival_status.vital_signs_present = true;
            state.survival_status.sos_panic_button_active = false;
            state.survival_status.cabin_pressure_nominal = true;
            soma_watchdog.reset_soma();
        } else if (msg == "INJECT_HYPOXIA") {
            state.telemetry_ridge.E_environment.spo2_pct = 91;
        } else if (msg == "INJECT_HYPERCAPNIA") {
            state.telemetry_ridge.E_environment.cabin_ppco2_mmHg = 4.1;
        } else if (msg == "INJECT_SANS") {
            state.telemetry_ridge.G_gravity.onsd_mm = 6.2;
        } else if (msg == "INJECT_SPE_RADIATION") {
            state.telemetry_ridge.R_radiation.acute_event_mSv = 285.0;
        } else if (msg == "INJECT_FEVER") {
            state.telemetry_ridge.E_environment.body_temperature_c = 39.2;
        } else if (msg == "INJECT_FATIGUE") {
            state.telemetry_ridge.I_isolation.sleep_hours_last_24h = 4.2;
            state.telemetry_ridge.I_isolation.hrv_rmssd_drop_pct = 31.0;
        } else if (msg == "RESET_NOMINAL") {
            state.survival_status.vital_signs_present = true;
            state.survival_status.sos_panic_button_active = false;
            state.survival_status.cabin_pressure_nominal = true;
            state.telemetry_ridge.R_radiation.acute_event_mSv = 8.5;
            state.telemetry_ridge.R_radiation.career_accumulated_mSv = 145.2;
            state.telemetry_ridge.I_isolation.sleep_hours_last_24h = 7.2;
            state.telemetry_ridge.I_isolation.hrv_rmssd_drop_pct = 12.0;
            state.telemetry_ridge.I_isolation.pvt_reaction_time_ms = 240;
            state.telemetry_ridge.G_gravity.heart_rate_bpm = 72;
            state.telemetry_ridge.G_gravity.blood_pressure_sys = 120;
            state.telemetry_ridge.G_gravity.blood_pressure_dia = 78;
            state.telemetry_ridge.G_gravity.onsd_mm = 4.8;
            state.telemetry_ridge.E_environment.cabin_ppco2_mmHg = 2.1;
            state.telemetry_ridge.E_environment.spo2_pct = 98;
            state.telemetry_ridge.E_environment.body_temperature_c = 36.8;
            soma_watchdog.reset_soma();
        }
    });

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> noise_dist(-0.2, 0.2);
    std::uniform_int_distribution<> hr_noise(-1, 1);

    std::cout << "[DAEMON RUNNING] Telemetry ingestion loop nominal @ 1 Hz (1000ms ticks)." << std::endl;
    std::cout << "[INFO] WebSocket stream active at ws://127.0.0.1:" << port << std::endl;
    std::cout << "[INFO] Connect Crew Dragon UI at file://f:/VitalGrid/web/index.html\n" << std::endl;

    uint64_t tick_counter = 0;

    while (g_keep_running.load()) {
        auto tick_start = std::chrono::steady_clock::now();

        RidgePayload current_sample;
        {
            std::lock_guard<std::mutex> lock(state_mutex);

            // Subtle physiological micro-fluctuations if alive
            if (state.survival_status.vital_signs_present) {
                state.telemetry_ridge.G_gravity.heart_rate_bpm = 
                    std::clamp(state.telemetry_ridge.G_gravity.heart_rate_bpm + hr_noise(gen), 40, 180);
                state.telemetry_ridge.E_environment.cabin_ppco2_mmHg = 
                    std::max(0.1, state.telemetry_ridge.E_environment.cabin_ppco2_mmHg + noise_dist(gen) * 0.05);
                state.telemetry_ridge.E_environment.body_temperature_c = 
                    std::clamp(state.telemetry_ridge.E_environment.body_temperature_c + noise_dist(gen) * 0.02, 34.0, 42.0);
            }

            state.timestamp = RidgePayload::current_iso8601();
            state.health_score = flight_surgeon.compute_health_score(state);
            current_sample = state;
        }

        // 1. Push to circular buffer
        telemetry_buffer.push(current_sample);

        // 2. Evaluate SOMA watchdog conditions
        soma_watchdog.evaluate_survival_state(current_sample);

        // 3. Evaluate clinical decision thresholds (Flight Surgeon AI)
        std::vector<ClinicalAlert> alerts = flight_surgeon.evaluate(current_sample);

        // 4. Broadcast RIDGE payload JSON over WebSocket
        std::string ridge_json = current_sample.to_json();
        ws_server.broadcast(ridge_json);

        // 5. Broadcast Clinical Alert sub-JSONs if thresholds breached
        for (const auto& alert : alerts) {
            std::string alert_json = alert.to_json();
            ws_server.broadcast(alert_json);
        }

        // Periodic console log every 5 seconds
        if (++tick_counter % 5 == 0) {
            std::cout << "[TELEMETRY TICK #" << tick_counter << "] Crew: " << current_sample.crew_member_id
                      << " | Score: " << current_sample.health_score << "%"
                      << " | HR: " << current_sample.telemetry_ridge.G_gravity.heart_rate_bpm << " bpm"
                      << " | SpO2: " << current_sample.telemetry_ridge.E_environment.spo2_pct << "%"
                      << " | ppCO2: " << current_sample.telemetry_ridge.E_environment.cabin_ppco2_mmHg << " mmHg"
                      << " | SOMA Active: " << (soma_watchdog.is_soma_triggered() ? "YES [DSOC LOCK]" : "NO [NOMINAL]")
                      << " | Alerts Active: " << alerts.size()
                      << " | WS Clients: " << ws_server.connected_clients_count()
                      << std::endl;
        }

        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tick_start);
        if (elapsed < std::chrono::milliseconds(1000)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000) - elapsed);
        }
    }

    std::cout << "[VITALGRID] Stopping SOMA Watchdog..." << std::endl;
    soma_watchdog.stop();

    std::cout << "[VITALGRID] Stopping WebSocket server..." << std::endl;
    ws_server.stop();

    std::cout << "[VITALGRID] Shutdown clean. All subsystems nominal." << std::endl;
    return 0;
}
