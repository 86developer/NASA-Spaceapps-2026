#pragma once

#include <string>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cmath>

namespace vitalgrid {

/**
 * @brief Survival status flags for immediate life-safety monitoring
 */
struct SurvivalStatus {
    bool vital_signs_present = true;
    bool sos_panic_button_active = false;
    bool cabin_pressure_nominal = true;

    std::string to_json() const {
        std::ostringstream ss;
        ss << "{"
           << "\"vital_signs_present\":" << (vital_signs_present ? "true" : "false") << ","
           << "\"sos_panic_button_active\":" << (sos_panic_button_active ? "true" : "false") << ","
           << "\"cabin_pressure_nominal\":" << (cabin_pressure_nominal ? "true" : "false")
           << "}";
        return ss.str();
    }
};

/**
 * @brief R - Radiation hazard metrics
 */
struct RadiationTelemetry {
    double career_accumulated_mSv = 145.2; // NASA career limit ~600 mSv
    double acute_event_mSv = 12.0;         // Solar Particle Event (SPE) acute limit > 250 mSv

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "{"
           << "\"career_accumulated_mSv\":" << career_accumulated_mSv << ","
           << "\"acute_event_mSv\":" << acute_event_mSv
           << "}";
        return ss.str();
    }
};

/**
 * @brief I - Isolation & Confinement psycho-neuro metrics
 */
struct IsolationTelemetry {
    double sleep_hours_last_24h = 5.8;    // Sleep duration
    double hrv_rmssd_drop_pct = 18.5;     // Autonomic nervous system stress index
    int pvt_reaction_time_ms = 280;       // Psychomotor Vigilance Task (PVT) ms
    int blink_rate_per_min = 18;          // Spontaneous blink rate per minute (eye fatigue indicator)
    int microsleep_events = 0;           // Cumulative microsleep episodes (>500ms involuntary closure)
    double perclos_pct = 0.0;            // Percentage of Eye Closure (PERCLOS) over moving window

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "{"
           << "\"sleep_hours_last_24h\":" << sleep_hours_last_24h << ","
           << "\"hrv_rmssd_drop_pct\":" << hrv_rmssd_drop_pct << ","
           << "\"pvt_reaction_time_ms\":" << pvt_reaction_time_ms << ","
           << "\"blink_rate_per_min\":" << blink_rate_per_min << ","
           << "\"microsleep_events\":" << microsleep_events << ","
           << "\"perclos_pct\":" << perclos_pct
           << "}";
        return ss.str();
    }
};

/**
 * @brief D - Distance from Earth logistics & communications metrics
 */
struct DistanceTelemetry {
    double comm_delay_oneway_min = 20.5;   // One-way Mars-to-Earth light delay in minutes
    int med_supply_days_remaining = 412;  // Autonomous medical supply inventory

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "{"
           << "\"comm_delay_oneway_min\":" << comm_delay_oneway_min << ","
           << "\"med_supply_days_remaining\":" << med_supply_days_remaining
           << "}";
        return ss.str();
    }
};

/**
 * @brief G - Gravity fields & physiological deconditioning metrics (Cardio & SANS)
 */
struct GravityTelemetry {
    int heart_rate_bpm = 105;
    int blood_pressure_sys = 135;
    int blood_pressure_dia = 85;
    double onsd_mm = 5.2;                   // Optic Nerve Sheath Diameter (SANS indicator: >5.8mm critical)
    double exercise_completed_hours = 1.5; // Daily countermeasure compliance

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "{"
           << "\"heart_rate_bpm\":" << heart_rate_bpm << ","
           << "\"blood_pressure_sys\":" << blood_pressure_sys << ","
           << "\"blood_pressure_dia\":" << blood_pressure_dia << ","
           << "\"onsd_mm\":" << onsd_mm << ","
           << "\"exercise_completed_hours\":" << exercise_completed_hours
           << "}";
        return ss.str();
    }
};

/**
 * @brief E - Hostile Closed Environment & Life Support metrics (ECLSS)
 */
struct EnvironmentTelemetry {
    double cabin_ppco2_mmHg = 3.2;      // Partial pressure CO2 (NASA nominal < 3.0 mmHg)
    int spo2_pct = 94;                 // Blood oxygen saturation (< 95% hypoxemia)
    double body_temperature_c = 37.9;   // Core temp (> 38.5 C space fever)

    std::string to_json() const {
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1);
        ss << "{"
           << "\"cabin_ppco2_mmHg\":" << cabin_ppco2_mmHg << ","
           << "\"spo2_pct\":" << spo2_pct << ","
           << "\"body_temperature_c\":" << body_temperature_c
           << "}";
        return ss.str();
    }
};

/**
 * @brief RIDGE composite telemetry structure
 */
struct TelemetryRIDGE {
    RadiationTelemetry R_radiation;
    IsolationTelemetry I_isolation;
    DistanceTelemetry D_distance;
    GravityTelemetry G_gravity;
    EnvironmentTelemetry E_environment;

    std::string to_json() const {
        std::ostringstream ss;
        ss << "{"
           << "\"R_radiation\":" << R_radiation.to_json() << ","
           << "\"I_isolation\":" << I_isolation.to_json() << ","
           << "\"D_distance\":" << D_distance.to_json() << ","
           << "\"G_gravity\":" << G_gravity.to_json() << ","
           << "\"E_environment\":" << E_environment.to_json()
           << "}";
        return ss.str();
    }
};

/**
 * @brief Top-level RIDGE Payload representing an astronaut state snapshot
 */
struct RidgePayload {
    std::string timestamp = "2026-10-02T12:33:24Z";
    std::string crew_member_id = "CMD-01";
    int health_score = 92;
    SurvivalStatus survival_status;
    TelemetryRIDGE telemetry_ridge;

    /**
     * @brief Generates current UTC ISO 8601 timestamp
     */
    static std::string current_iso8601() {
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
#if defined(_WIN32) || defined(_WIN64)
        gmtime_s(&tm_buf, &now_c);
#else
        gmtime_r(&now_c, &tm_buf);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        return std::string(buf);
    }

    /**
     * @brief Exact JSON serialization compliant with NASA RIDGE specification
     */
    std::string to_json() const {
        std::ostringstream ss;
        ss << "{"
           << "\"timestamp\":\"" << timestamp << "\","
           << "\"crew_member_id\":\"" << crew_member_id << "\","
           << "\"health_score\":" << health_score << ","
           << "\"survival_status\":" << survival_status.to_json() << ","
           << "\"telemetry_ridge\":" << telemetry_ridge.to_json()
           << "}";
        return ss.str();
    }
};

} // namespace vitalgrid
