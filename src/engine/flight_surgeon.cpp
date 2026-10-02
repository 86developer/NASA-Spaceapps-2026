#include "flight_surgeon.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace vitalgrid {

std::vector<ClinicalAlert> FlightSurgeonAI::evaluate(const RidgePayload& payload) {
    std::vector<ClinicalAlert> alerts;
    const auto& r = payload.telemetry_ridge.R_radiation;
    const auto& i = payload.telemetry_ridge.I_isolation;
    const auto& g = payload.telemetry_ridge.G_gravity;
    const auto& e = payload.telemetry_ridge.E_environment;
    const std::string ts = payload.timestamp;

    // 1. CRITICAL: Radiation acute > 250 mSv or career > 600 mSv
    if (r.acute_event_mSv > THRESHOLD_ACUTE_RAD_CRITICAL_MSV) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "acute_event_mSv";
        alert.trigger_value = std::to_string(r.acute_event_mSv) + " mSv";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_ACUTE_RAD_CRITICAL_MSV) + " mSv";
        alert.diagnosis = "ACUTE RADIATION SYNDROME (ARS) - SOLAR PARTICLE EVENT (SPE) MASSIVE EXPOSURE";
        alert.tactical_action = "IMMEDIATE EVACUATION to deep storm shelter core (water wall shielded compartment). Administer Filgrastim (G-CSF) 300mcg SQ. Power down non-vital forward bus avionics.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (r.career_accumulated_mSv > THRESHOLD_CAREER_RAD_CRITICAL_MSV) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "career_accumulated_mSv";
        alert.trigger_value = std::to_string(r.career_accumulated_mSv) + " mSv";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_CAREER_RAD_CRITICAL_MSV) + " mSv";
        alert.diagnosis = "CUMULATIVE LIFETIME IONIZING RADIATION CEILING EXCEEDED";
        alert.tactical_action = "Terminate all EVA operations permanently. Reassign to high-shielded central habitat watch. Initiate daily peripheral blood lymphocyte chromosomal assays.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 2. CRITICAL: Blood Pressure > 155/95 mmHg, HR > 110 bpm, or ONSD > 5.8 mm
    if (g.blood_pressure_sys > THRESHOLD_BP_SYS_CRITICAL || g.blood_pressure_dia > THRESHOLD_BP_DIA_CRITICAL) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "blood_pressure";
        alert.trigger_value = std::to_string(g.blood_pressure_sys) + "/" + std::to_string(g.blood_pressure_dia) + " mmHg";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_BP_SYS_CRITICAL) + "/" + std::to_string(THRESHOLD_BP_DIA_CRITICAL) + " mmHg";
        alert.diagnosis = "HYPERTENSIVE CRISIS UNDER MICROGRAVITY - CEREBROVASCULAR RISK";
        alert.tactical_action = "Cease all aerobic/resistive exercises. Secure crew in supine restraint. Administer sublingual Captopril 25mg; repeat BP every 10 min. Monitor for intracranial hemorrhage signs.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (g.heart_rate_bpm > THRESHOLD_HR_CRITICAL_BPM) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "heart_rate_bpm";
        alert.trigger_value = std::to_string(g.heart_rate_bpm) + " bpm";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_HR_CRITICAL_BPM) + " bpm";
        alert.diagnosis = "SUSTAINED SUPRAVENTRICULAR TACHYCARDIA / AUTONOMIC DYSREGULATION";
        alert.tactical_action = "Perform vagal maneuvers (modified Valsalva maneuver). Verify hydration state and electrolyte telemetry (K+/Mg2+). Prepare IV Metoprolol 5mg if persistent > 15 min.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (g.onsd_mm > THRESHOLD_ONSD_CRITICAL_MM) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "onsd_mm";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << g.onsd_mm << " mm";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_ONSD_CRITICAL_MM) + " mm";
        alert.diagnosis = "SPACEFLIGHT-ASSOCIATED NEURO-OCULAR SYNDROME (SANS) - ELEVATED INTRACRANIAL PRESSURE (ICP)";
        alert.tactical_action = "Deploy Lower Body Negative Pressure (LBNP) device at -35 mmHg for 2 hours to redistribute cephalad fluid caudal. Administer oral Acetazolamide 250mg PO. Schedule high-res funduscopy.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 3. CRITICAL: ppCO2 > 3.0 mmHg (hipercapnia) or SpO2 < 95% (hipoxia), Temp > 38.5 C (fiebre espacial)
    if (e.cabin_ppco2_mmHg > THRESHOLD_PPCO2_CRITICAL_MMHG) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "cabin_ppco2_mmHg";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << e.cabin_ppco2_mmHg << " mmHg";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_PPCO2_CRITICAL_MMHG) + " mmHg";
        alert.diagnosis = "CABIN ATMOSPHERIC HYPERCAPNIA - CO2 RETENTION / ECLSS DEGRADATION";
        alert.tactical_action = "Cycle Carbon Dioxide Removal Assembly (CDRA) beds to Fast Desorption mode. Don emergency crew O2 masks. Activate LiOH backup chemical scrubber cartridges in sleep station.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (e.spo2_pct < THRESHOLD_SPO2_CRITICAL_PCT) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "spo2_pct";
        alert.trigger_value = std::to_string(e.spo2_pct) + "%";
        alert.threshold_limit = "< " + std::to_string(THRESHOLD_SPO2_CRITICAL_PCT) + "%";
        alert.diagnosis = "ACUTE SYSTEMIC HYPOXEMIA / PULMONARY V/Q MISMATCH";
        alert.tactical_action = "Deliver 100% O2 via aviator mask at 6 L/min immediately. Inspect suit/cabin O2 partial pressure sensors. Auscultate lung bases with digital stethoscope for atelectasis.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (e.body_temperature_c > THRESHOLD_BODY_TEMP_CRITICAL_C) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "body_temperature_c";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << e.body_temperature_c << " °C";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_BODY_TEMP_CRITICAL_C) + " °C";
        alert.diagnosis = "SPACE PYREXIA / MALIGNANT HYPERTHERMIA OR DEEP SPACE SEPSIS";
        alert.tactical_action = "Engage Liquid Cooling and Ventilation Garment (LCVG) in active chiller loop. Infuse IV Paracetamol 1000mg. Draw blood samples into Bio-Analyzer cartridge for sepsis panel.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 4. WARNING: Sleep < 6.0h or HRV RMSSD drop > 25%
    if (i.sleep_hours_last_24h < THRESHOLD_SLEEP_WARN_HOURS) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "sleep_hours_last_24h";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << i.sleep_hours_last_24h << " h";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "< " + std::to_string(THRESHOLD_SLEEP_WARN_HOURS) + " h";
        alert.diagnosis = "CHRONIC SLEEP DEBT / COGNITIVE FATIGUE IMPAIRMENT RISK";
        alert.tactical_action = "Prescribe mandatory 60-min restorative rest in darkened crew quarters. Shift ambient lighting to 480nm-depleted warm circadian spectrum. Hold safety-critical robotics tasks.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (i.hrv_rmssd_drop_pct > THRESHOLD_HRV_DROP_WARN_PCT) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "hrv_rmssd_drop_pct";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << i.hrv_rmssd_drop_pct << "%";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_HRV_DROP_WARN_PCT) + "%";
        alert.diagnosis = "AUTONOMIC SYMPATHETIC DOMINANCE & HIGH ALLORITHMIC STRESS LOAD";
        alert.tactical_action = "Initiate guided biofeedback resonance breathing (0.1 Hz pacing) for 15 min. Reduce sensory input in workstation. Assess cortisol/melatonin salivary biomarkers.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    return alerts;
}

int FlightSurgeonAI::compute_health_score(const RidgePayload& payload) const {
    double score = 100.0;
    const auto& r = payload.telemetry_ridge.R_radiation;
    const auto& i = payload.telemetry_ridge.I_isolation;
    const auto& g = payload.telemetry_ridge.G_gravity;
    const auto& e = payload.telemetry_ridge.E_environment;
    const auto& s = payload.survival_status;

    // Survival compromises = instantaneous score drop
    if (!s.vital_signs_present) return 0;
    if (s.sos_panic_button_active) score -= 35.0;
    if (!s.cabin_pressure_nominal) score -= 40.0;

    // Radiation penalties
    if (r.acute_event_mSv > 250.0) score -= 40.0;
    else if (r.acute_event_mSv > 50.0) score -= 15.0;
    if (r.career_accumulated_mSv > 600.0) score -= 25.0;

    // Gravity / Cardiovascular penalties
    if (g.heart_rate_bpm > 110) score -= 15.0;
    if (g.blood_pressure_sys > 155 || g.blood_pressure_dia > 95) score -= 15.0;
    if (g.onsd_mm > 5.8) score -= 20.0;
    else if (g.onsd_mm > 5.0) score -= 5.0;

    // Environmental penalties
    if (e.cabin_ppco2_mmHg > 3.0) score -= 20.0;
    else if (e.cabin_ppco2_mmHg > 2.0) score -= 5.0;
    if (e.spo2_pct < 95) score -= 25.0;
    if (e.body_temperature_c > 38.5) score -= 20.0;

    // Isolation / Stress penalties
    if (i.sleep_hours_last_24h < 6.0) score -= 8.0;
    if (i.hrv_rmssd_drop_pct > 25.0) score -= 8.0;
    if (i.pvt_reaction_time_ms > 350) score -= 5.0;

    score = std::clamp(score, 0.0, 100.0);
    return static_cast<int>(std::round(score));
}

} // namespace vitalgrid
