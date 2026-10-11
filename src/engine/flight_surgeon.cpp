#include "flight_surgeon.hpp"
#include "json_min.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
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
    if (r.acute_event_mSv > limits_.acute_mSv) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "acute_event_mSv";
        alert.trigger_value = std::to_string(r.acute_event_mSv) + " mSv";
        alert.threshold_limit = "> " + std::to_string(limits_.acute_mSv) + " mSv";
        alert.diagnosis = "SÍNDROME DE RADIACIÓN AGUDA (ARS) - EXPOSICIÓN MASIVA A EVENTO DE PARTÍCULAS SOLARES (SPE)";
        alert.tactical_action = "EVACUACIÓN INMEDIATA al núcleo blindado (Storm Shelter con paredes de agua). Administrar Filgrastim (G-CSF) 300mcg subcutáneo. Desenergizar aviónica delantera no esencial.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (r.career_accumulated_mSv > limits_.career_mSv) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "career_accumulated_mSv";
        alert.trigger_value = std::to_string(r.career_accumulated_mSv) + " mSv";
        alert.threshold_limit = "> " + std::to_string(limits_.career_mSv) + " mSv";
        alert.diagnosis = "LÍMITE MÁXIMO DE RADIACIÓN IONIZANTE DE CARRERA EXCEDIDO";
        alert.tactical_action = "Suspender permanentemente todas las actividades extravehiculares (EVA). Reasignar a guardia en hábitat central de alto blindaje. Iniciar análisis cromosómicos diarios en linfocitos.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 2. CRITICAL: Blood Pressure > 155/95 mmHg, HR > 110 bpm, or ONSD > 5.8 mm
    if (g.blood_pressure_sys > limits_.bp_sys || g.blood_pressure_dia > limits_.bp_dia) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "blood_pressure";
        alert.trigger_value = std::to_string(g.blood_pressure_sys) + "/" + std::to_string(g.blood_pressure_dia) + " mmHg";
        alert.threshold_limit = "> " + std::to_string(limits_.bp_sys) + "/" + std::to_string(limits_.bp_dia) + " mmHg";
        alert.diagnosis = "CRISIS HIPERTENSIVA EN MICROGRAVEDAD - RIESGO CEREBROVASCULAR";
        alert.tactical_action = "Suspender todo ejercicio aeróbico y resistivo. Asegurar a la tripulación en posición supina. Administrar Captopril 25mg sublingual; repetir control de presión arterial cada 10 min. Vigilar signos de hemorragia intracraneal.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (g.heart_rate_bpm > limits_.hr_bpm) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "heart_rate_bpm";
        alert.trigger_value = std::to_string(g.heart_rate_bpm) + " bpm";
        alert.threshold_limit = "> " + std::to_string(limits_.hr_bpm) + " bpm";
        alert.diagnosis = "TAQUICARDIA SUPRAVENTRICULAR SOSTENIDA / DESREGULACIÓN AUTÓNOMA";
        alert.tactical_action = "Realizar maniobras vagales (maniobra de Valsalva modificada). Verificar estado de hidratación y telemetría de electrolitos (K+/Mg2+). Preparar Metoprolol 5mg IV si persiste > 15 min.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (g.onsd_mm > limits_.onsd_mm) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "onsd_mm";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << g.onsd_mm << " mm";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(limits_.onsd_mm) + " mm";
        alert.diagnosis = "SÍNDROME NEURO-OCULAR ASOCIADO AL VUELO ESPACIAL (SANS) - PRESIÓN INTRACRANEAL (ICP) CRÍTICA";
        alert.tactical_action = "Desplegar dispositivo de Presión Negativa en Miembros Inferiores (LBNP) a -35 mmHg por 2 horas para redistribución caudal del fluido cefálico. Administrar Acetazolamida 250mg VO. Programar fondo de ojo de alta resolución.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 3. CRITICAL: ppCO2 > 3.0 mmHg (hipercapnia) or SpO2 < 95% (hipoxia), Temp > 38.5 C (fiebre espacial)
    if (e.cabin_ppco2_mmHg > limits_.ppco2) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "cabin_ppco2_mmHg";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(2) << e.cabin_ppco2_mmHg << " mmHg";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(limits_.ppco2) + " mmHg";
        alert.diagnosis = "HIPERCAPNIA ATMOSFÉRICA EN CABINA - RETENCIÓN DE CO2 / DEGRADACIÓN ECLSS";
        alert.tactical_action = "Ciclar lechos del Ensamblaje de Eliminación de Dióxido de Carbono (CDRA) a modo de desorción rápida de emergencia. Colocar máscaras de O2 a la tripulación. Activar cartuchos químicos de LiOH de respaldo en cabina.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (e.spo2_pct < limits_.spo2) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "spo2_pct";
        alert.trigger_value = std::to_string(e.spo2_pct) + "%";
        alert.threshold_limit = "< " + std::to_string(limits_.spo2) + "%";
        alert.diagnosis = "HIPOXEMIA SISTÉMICA AGUDA / DESEQUILIBRIO VENTILACIÓN-PERFUSIÓN (V/Q)";
        alert.tactical_action = "Suministrar O2 al 100% mediante mascarilla de aviador a 6 L/min inmediatamente. Inspeccionar sensores de presión parcial de O2 en traje y cabina. Auscultar bases pulmonares para descartar atelectasia.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (e.body_temperature_c > limits_.temp_c) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::CRITICAL;
        alert.trigger_metric = "body_temperature_c";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << e.body_temperature_c << " °C";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(limits_.temp_c) + " °C";
        alert.diagnosis = "FIEBRE ESPACIAL / HIPERTERMIA MALIGNA O SEPSIS DE ESPACIO PROFUNDO";
        alert.tactical_action = "Conectar Prenda de Ventilación y Enfriamiento Líquido (LCVG) en bucle activo de refrigeración. Infundir Paracetamol 1000mg IV. Extraer muestras de sangre en cartucho de bioanalizador para panel de sepsis.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 4. WARNING: Sleep < 6.0h or HRV RMSSD drop > 25%
    if (i.sleep_hours_last_24h < limits_.sleep_h) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "sleep_hours_last_24h";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << i.sleep_hours_last_24h << " h";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "< " + std::to_string(limits_.sleep_h) + " h";
        alert.diagnosis = "DEUDA CRÓNICA DE SUEÑO / RIESGO DE DETERIORO POR FATIGA COGNITIVA";
        alert.tactical_action = "Prescribir reposo reparador obligatorio de 60 min en camarote oscurecido. Cambiar iluminación ambiental a espectro circadiano cálido sin componente azul de 480nm. Suspender tareas robóticas de alta criticidad.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (i.hrv_rmssd_drop_pct > limits_.hrv_pct) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "hrv_rmssd_drop_pct";
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << i.hrv_rmssd_drop_pct << "%";
        alert.trigger_value = ss.str();
        alert.threshold_limit = "> " + std::to_string(limits_.hrv_pct) + "%";
        alert.diagnosis = "DOMINANCIA SIMPÁTICA AUTÓNOMA Y ELEVADA CARGA DE ESTRÉS ALOSTÁTICO";
        alert.tactical_action = "Iniciar biorretroalimentación de respiración resonante guiada (marcapasos a 0.1 Hz) durante 15 min. Reducir estímulos sensoriales en la estación de trabajo. Evaluar biomarcadores salivales de cortisol y melatonina.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    // 5. WARNING: Severe blink rate drop (< 8 / min) or microsleep events detected (>= 1)
    if (i.blink_rate_per_min < limits_.blink_per_min) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "blink_rate_per_min";
        alert.trigger_value = std::to_string(i.blink_rate_per_min) + " parp/min";
        alert.threshold_limit = "< " + std::to_string(limits_.blink_per_min) + " parp/min";
        alert.diagnosis = "FATIGA OCULAR SEVERA / HIPOVIGILANCIA POR CAÍDA DE FRECUENCIA DE PARPADEO";
        alert.tactical_action = "Activar protocolo de descanso visual 20-20-20. Suministrar gotas oftálmicas lubricantes en cabina. Suspender tareas robóticas de precisión y evaluar nivel de fatiga neurocognitiva.";
        alert.timestamp = ts;
        alerts.push_back(alert);
    }

    if (i.microsleep_events >= limits_.microsleep) {
        ClinicalAlert alert;
        alert.status = AlertSeverity::WARNING;
        alert.trigger_metric = "microsleep_events";
        alert.trigger_value = std::to_string(i.microsleep_events) + " eventos";
        alert.threshold_limit = ">= " + std::to_string(limits_.microsleep);
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

    const auto& L = limits_;

    // Survival compromises = instantaneous score drop
    if (!s.vital_signs_present) return 0;
    if (s.sos_panic_button_active) score -= L.pen_sos;
    if (!s.cabin_pressure_nominal) score -= L.pen_depress;

    // Radiation penalties
    if (r.acute_event_mSv > L.acute_mSv) score -= L.pen_acute;
    else if (r.acute_event_mSv > L.acute_mid_mSv) score -= L.pen_acute_mid;
    if (r.career_accumulated_mSv > L.career_mSv) score -= L.pen_career;

    // Gravity / Cardiovascular penalties
    if (g.heart_rate_bpm > L.hr_bpm) score -= L.pen_hr;
    if (g.blood_pressure_sys > L.bp_sys || g.blood_pressure_dia > L.bp_dia) score -= L.pen_bp;
    if (g.onsd_mm > L.onsd_mm) score -= L.pen_onsd;
    else if (g.onsd_mm > L.onsd_soft_mm) score -= L.pen_onsd_soft;

    // Environmental penalties
    if (e.cabin_ppco2_mmHg > L.ppco2) score -= L.pen_ppco2;
    else if (e.cabin_ppco2_mmHg > L.ppco2_soft) score -= L.pen_ppco2_soft;
    if (e.spo2_pct < L.spo2) score -= L.pen_spo2;
    if (e.body_temperature_c > L.temp_c) score -= L.pen_temp;

    // Isolation / Stress penalties
    if (i.sleep_hours_last_24h < L.sleep_h) score -= L.pen_sleep;
    if (i.hrv_rmssd_drop_pct > L.hrv_pct) score -= L.pen_hrv;
    if (i.pvt_reaction_time_ms > L.pvt_ms) score -= L.pen_pvt;
    if (i.blink_rate_per_min < L.blink_per_min) score -= L.pen_blink;
    if (i.microsleep_events >= L.microsleep) score -= L.pen_microsleep;

    score = std::clamp(score, 0.0, 100.0);
    return static_cast<int>(std::round(score));
}

namespace {

bool assign_number(const Json& object, const char* key, double& destination) {
    const Json* value = object.find(key);
    if (!value || value->type != Json::Type::Number) return false;
    destination = value->number;
    return true;
}

} // namespace

bool FlightSurgeonAI::load_thresholds(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::stringstream buffer;
    buffer << input.rdbuf();

    Json root;
    try {
        root = parse_json(buffer.str());
    } catch (const std::exception& ex) {
        std::cerr << "[FLIGHT SURGEON] No se leyeron los umbrales (" << ex.what() << "). Siguen los valores compilados.\n";
        return false;
    }

    const Json* limits = root.find("limits");
    const Json* score = root.find("score");
    const Json* agent = root.find("agent");
    if (!limits || !score || limits->type != Json::Type::Object || score->type != Json::Type::Object) {
        std::cerr << "[FLIGHT SURGEON] El archivo no tiene limits y score. Siguen los valores compilados.\n";
        return false;
    }

    Limits loaded = limits_;
    const std::pair<const char*, double Limits::*> limit_fields[] = {
        {"acute_event_mSv", &Limits::acute_mSv},
        {"acute_event_mSv_mid", &Limits::acute_mid_mSv},
        {"career_accumulated_mSv", &Limits::career_mSv},
        {"blood_pressure_sys", &Limits::bp_sys},
        {"blood_pressure_dia", &Limits::bp_dia},
        {"heart_rate_bpm", &Limits::hr_bpm},
        {"onsd_mm", &Limits::onsd_mm},
        {"onsd_mm_soft", &Limits::onsd_soft_mm},
        {"cabin_ppco2_mmHg", &Limits::ppco2},
        {"cabin_ppco2_mmHg_soft", &Limits::ppco2_soft},
        {"spo2_pct", &Limits::spo2},
        {"body_temperature_c", &Limits::temp_c},
        {"sleep_hours_last_24h", &Limits::sleep_h},
        {"hrv_rmssd_drop_pct", &Limits::hrv_pct},
        {"blink_rate_per_min", &Limits::blink_per_min},
        {"microsleep_events", &Limits::microsleep},
        {"pvt_reaction_time_ms", &Limits::pvt_ms},
    };
    const std::pair<const char*, double Limits::*> score_fields[] = {
        {"sos", &Limits::pen_sos},
        {"depressurization", &Limits::pen_depress},
        {"acute_event_mSv", &Limits::pen_acute},
        {"acute_event_mSv_mid", &Limits::pen_acute_mid},
        {"career_accumulated_mSv", &Limits::pen_career},
        {"blood_pressure", &Limits::pen_bp},
        {"heart_rate_bpm", &Limits::pen_hr},
        {"onsd_mm", &Limits::pen_onsd},
        {"onsd_mm_soft", &Limits::pen_onsd_soft},
        {"cabin_ppco2_mmHg", &Limits::pen_ppco2},
        {"cabin_ppco2_mmHg_soft", &Limits::pen_ppco2_soft},
        {"spo2_pct", &Limits::pen_spo2},
        {"body_temperature_c", &Limits::pen_temp},
        {"sleep_hours_last_24h", &Limits::pen_sleep},
        {"hrv_rmssd_drop_pct", &Limits::pen_hrv},
        {"blink_rate_per_min", &Limits::pen_blink},
        {"microsleep_events", &Limits::pen_microsleep},
        {"pvt_reaction_time_ms", &Limits::pen_pvt},
    };

    for (const auto& field : limit_fields) {
        if (!assign_number(*limits, field.first, loaded.*(field.second))) {
            std::cerr << "[FLIGHT SURGEON] Falta el limite " << field.first << ".\n";
            return false;
        }
    }
    for (const auto& field : score_fields) {
        if (!assign_number(*score, field.first, loaded.*(field.second))) {
            std::cerr << "[FLIGHT SURGEON] Falta la penalizacion " << field.first << ".\n";
            return false;
        }
    }

    limits_ = loaded;
    std::string name = "Médico de vuelo";
    if (agent && agent->type == Json::Type::Object) {
        if (const Json* agent_name = agent->find("name")) {
            if (agent_name->type == Json::Type::String && !agent_name->text.empty()) {
                name = agent_name->text;
            }
        }
    }
    std::cout << "[FLIGHT SURGEON] Un agente (" << name
              << "). Umbrales cargados desde " << path << std::endl;
    return true;
}

} // namespace vitalgrid
