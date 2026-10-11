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
        alert.diagnosis = "SÍNDROME DE RADIACIÓN AGUDA (ARS) - EXPOSICIÓN MASIVA A EVENTO DE PARTÍCULAS SOLARES (SPE)";
        alert.tactical_action = "EVACUACIÓN INMEDIATA al núcleo blindado (Storm Shelter con paredes de agua). Administrar Filgrastim (G-CSF) 300mcg subcutáneo. Desenergizar aviónica delantera no esencial.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (r.career_accumulated_mSv > THRESHOLD_CAREER_RAD_CRITICAL_MSV) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "career_accumulated_mSv";
        alert.trigger_value = std::to_string(r.career_accumulated_mSv) + " mSv";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_CAREER_RAD_CRITICAL_MSV) + " mSv";
        alert.diagnosis = "LÍMITE MÁXIMO DE RADIACIÓN IONIZANTE DE CARRERA EXCEDIDO";
        alert.tactical_action = "Suspender permanentemente todas las actividades extravehiculares (EVA). Reasignar a guardia en hábitat central de alto blindaje. Iniciar análisis cromosómicos diarios en linfocitos.";
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
        alert.diagnosis = "CRISIS HIPERTENSIVA EN MICROGRAVEDAD - RIESGO CEREBROVASCULAR";
        alert.tactical_action = "Suspender todo ejercicio aeróbico y resistivo. Asegurar a la tripulación en posición supina. Administrar Captopril 25mg sublingual; repetir control de presión arterial cada 10 min. Vigilar signos de hemorragia intracraneal.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (g.heart_rate_bpm > THRESHOLD_HR_CRITICAL_BPM) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "heart_rate_bpm";
        alert.trigger_value = std::to_string(g.heart_rate_bpm) + " bpm";
        alert.threshold_limit = "> " + std::to_string(THRESHOLD_HR_CRITICAL_BPM) + " bpm";
        alert.diagnosis = "TAQUICARDIA SUPRAVENTRICULAR SOSTENIDA / DESREGULACIÓN AUTÓNOMA";
        alert.tactical_action = "Realizar maniobras vagales (maniobra de Valsalva modificada). Verificar estado de hidratación y telemetría de electrolitos (K+/Mg2+). Preparar Metoprolol 5mg IV si persiste > 15 min.";
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
        alert.diagnosis = "SÍNDROME NEURO-OCULAR ASOCIADO AL VUELO ESPACIAL (SANS) - PRESIÓN INTRACRANEAL (ICP) CRÍTICA";
        alert.tactical_action = "Desplegar dispositivo de Presión Negativa en Miembros Inferiores (LBNP) a -35 mmHg por 2 horas para redistribución caudal del fluido cefálico. Administrar Acetazolamida 250mg VO. Programar fondo de ojo de alta resolución.";
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
        alert.diagnosis = "HIPERCAPNIA ATMOSFÉRICA EN CABINA - RETENCIÓN DE CO2 / DEGRADACIÓN ECLSS";
        alert.tactical_action = "Ciclar lechos del Ensamblaje de Eliminación de Dióxido de Carbono (CDRA) a modo de desorción rápida de emergencia. Colocar máscaras de O2 a la tripulación. Activar cartuchos químicos de LiOH de respaldo en cabina.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (e.spo2_pct < THRESHOLD_SPO2_CRITICAL_PCT) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "spo2_pct";
        alert.trigger_value = std::to_string(e.spo2_pct) + "%";
        alert.threshold_limit = "< " + std::to_string(THRESHOLD_SPO2_CRITICAL_PCT) + "%";
        alert.diagnosis = "HIPOXEMIA SISTÉMICA AGUDA / DESEQUILIBRIO VENTILACIÓN-PERFUSIÓN (V/Q)";
        alert.tactical_action = "Suministrar O2 al 100% mediante mascarilla de aviador a 6 L/min inmediatamente. Inspeccionar sensores de presión parcial de O2 en traje y cabina. Auscultar bases pulmonares para descartar atelectasia.";
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
        alert.diagnosis = "FIEBRE ESPACIAL / HIPERTERMIA MALIGNA O SEPSIS DE ESPACIO PROFUNDO";
        alert.tactical_action = "Conectar Prenda de Ventilación y Enfriamiento Líquido (LCVG) en bucle activo de refrigeración. Infundir Paracetamol 1000mg IV. Extraer muestras de sangre en cartucho de bioanalizador para panel de sepsis.";
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
        alert.diagnosis = "DEUDA CRÓNICA DE SUEÑO / RIESGO DE DETERIORO POR FATIGA COGNITIVA";
        alert.tactical_action = "Prescribir reposo reparador obligatorio de 60 min en camarote oscurecido. Cambiar iluminación ambiental a espectro circadiano cálido sin componente azul de 480nm. Suspender tareas robóticas de alta criticidad.";
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
        alert.diagnosis = "DOMINANCIA SIMPÁTICA AUTÓNOMA Y ELEVADA CARGA DE ESTRÉS ALOSTÁTICO";
        alert.tactical_action = "Iniciar biorretroalimentación de respiración resonante guiada (marcapasos a 0.1 Hz) durante 15 min. Reducir estímulos sensoriales en la estación de trabajo. Evaluar biomarcadores salivales de cortisol y melatonina.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 5. WARNING: Severe blink rate drop (< 8 / min) or microsleep events detected (>= 1)
    if (i.blink_rate_per_min < THRESHOLD_BLINK_RATE_WARN_PER_MIN) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "blink_rate_per_min";
        alert.trigger_value = std::to_string(i.blink_rate_per_min) + " parp/min";
        alert.threshold_limit = "< " + std::to_string(THRESHOLD_BLINK_RATE_WARN_PER_MIN) + " parp/min";
        alert.diagnosis = "FATIGA OCULAR SEVERA / HIPOVIGILANCIA POR CAÍDA DE FRECUENCIA DE PARPADEO";
        alert.tactical_action = "Activar protocolo de descanso visual 20-20-20. Suministrar gotas oftálmicas lubricantes en cabina. Suspender tareas robóticas de precisión y evaluar nivel de fatiga neurocognitiva.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (i.microsleep_events >= THRESHOLD_MICROSLEEP_WARN_COUNT) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "microsleep_events";
        alert.trigger_value = std::to_string(i.microsleep_events) + " eventos";
        alert.threshold_limit = ">= " + std::to_string(THRESHOLD_MICROSLEEP_WARN_COUNT);
        alert.diagnosis = "EVENTO DE MICROSUEÑO DETECTADO - PÉRDIDA TRANSITORIA DE CONCIENCIA";
        alert.tactical_action = "EMITIR ALERTA AUDITIVA EN CABINA (Chime de atención inmediata). Notificar al comandante de misión. Relevar inmediatamente al astronauta de controles de vuelo activos.";
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
    if (i.blink_rate_per_min < 8) score -= 8.0;
    if (i.microsleep_events > 0) score -= 15.0;

    score = std::clamp(score, 0.0, 100.0);
    return static_cast<int>(std::round(score));
}

} // namespace vitalgrid
