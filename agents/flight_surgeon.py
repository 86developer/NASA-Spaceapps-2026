"""Un solo agente: el médico de vuelo.

Cubre radiación, aislamiento, distancia, gravedad y ambiente con la misma
tabla. No hay un agente por pilar ni un coordinador aparte. Los números del
plan salen de la telemetría o de config/clinical_thresholds.json. Las citas
salen del cache de OSDR; si el estudio no está ahí, no se inventa.
"""

from __future__ import annotations

import argparse
import base64
import json
import re
import socket
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CONFIG = ROOT / "config" / "clinical_thresholds.json"
DEFAULT_OSDR = ROOT / "data" / "osdr_live_cache.json"

METRIC_PATHS = {
    "acute_event_mSv": ("R_radiation", "acute_event_mSv"),
    "career_accumulated_mSv": ("R_radiation", "career_accumulated_mSv"),
    "blood_pressure_sys": ("G_gravity", "blood_pressure_sys"),
    "blood_pressure_dia": ("G_gravity", "blood_pressure_dia"),
    "heart_rate_bpm": ("G_gravity", "heart_rate_bpm"),
    "onsd_mm": ("G_gravity", "onsd_mm"),
    "cabin_ppco2_mmHg": ("E_environment", "cabin_ppco2_mmHg"),
    "spo2_pct": ("E_environment", "spo2_pct"),
    "body_temperature_c": ("E_environment", "body_temperature_c"),
    "sleep_hours_last_24h": ("I_isolation", "sleep_hours_last_24h"),
    "hrv_rmssd_drop_pct": ("I_isolation", "hrv_rmssd_drop_pct"),
    "blink_rate_per_min": ("I_isolation", "blink_rate_per_min"),
    "microsleep_events": ("I_isolation", "microsleep_events"),
    "pvt_reaction_time_ms": ("I_isolation", "pvt_reaction_time_ms"),
}

ACCESSION = re.compile(r"\b(?:OSD|GLDS)-\d+\b")
NUMBER = re.compile(r"-?\d+(?:\.\d+)?")


def load_config(path: Path = DEFAULT_CONFIG) -> dict:
    config = json.loads(path.read_text(encoding="utf-8"))
    agent = config.get("agent") or {}
    if agent.get("count") != 1 or agent.get("id") != "flight_surgeon":
        raise ValueError("La configuracion tiene que declarar un solo agente: flight_surgeon.")
    if "limits" not in config or "score" not in config or "rules" not in config:
        raise ValueError("Faltan limits, score o rules.")
    return config


def metric_value(payload: dict, metric: str):
    ridge = payload.get("telemetry_ridge") or {}
    group, key = METRIC_PATHS[metric]
    block = ridge.get(group) or {}
    if key not in block:
        return None
    return block[key]


def _compare(value, op: str, limit: float) -> bool:
    if value is None:
        return False
    if op == "gt":
        return value > limit
    if op == "lt":
        return value < limit
    if op == "ge":
        return value >= limit
    raise ValueError(f"operador desconocido: {op}")


def _rule_terms(rule: dict) -> list[dict]:
    if rule.get("match") == "any":
        return rule["terms"]
    return [{"metric": rule["metric"], "op": rule["op"], "limit_key": rule["limit_key"]}]


def rule_triggered(rule: dict, payload: dict, limits: dict) -> bool:
    return any(
        _compare(metric_value(payload, term["metric"]), term["op"], limits[term["limit_key"]])
        for term in _rule_terms(rule)
    )


def health_score(payload: dict, config: dict) -> int:
    """Misma formula que FlightSurgeonAI::compute_health_score."""
    limits = config["limits"]
    pen = config["score"]
    survival = payload.get("survival_status") or {}
    ridge = payload.get("telemetry_ridge") or {}
    radiation = ridge.get("R_radiation") or {}
    gravity = ridge.get("G_gravity") or {}
    environment = ridge.get("E_environment") or {}
    isolation = ridge.get("I_isolation") or {}

    if not survival.get("vital_signs_present", True):
        return 0

    score = 100.0
    if survival.get("sos_panic_button_active"):
        score -= pen["sos"]
    if not survival.get("cabin_pressure_nominal", True):
        score -= pen["depressurization"]

    acute = radiation.get("acute_event_mSv", 0)
    if acute > limits["acute_event_mSv"]:
        score -= pen["acute_event_mSv"]
    elif acute > limits["acute_event_mSv_mid"]:
        score -= pen["acute_event_mSv_mid"]
    if radiation.get("career_accumulated_mSv", 0) > limits["career_accumulated_mSv"]:
        score -= pen["career_accumulated_mSv"]

    if gravity.get("heart_rate_bpm", 0) > limits["heart_rate_bpm"]:
        score -= pen["heart_rate_bpm"]
    if (
        gravity.get("blood_pressure_sys", 0) > limits["blood_pressure_sys"]
        or gravity.get("blood_pressure_dia", 0) > limits["blood_pressure_dia"]
    ):
        score -= pen["blood_pressure"]
    onsd = gravity.get("onsd_mm", 0)
    if onsd > limits["onsd_mm"]:
        score -= pen["onsd_mm"]
    elif onsd > limits["onsd_mm_soft"]:
        score -= pen["onsd_mm_soft"]

    ppco2 = environment.get("cabin_ppco2_mmHg", 0)
    if ppco2 > limits["cabin_ppco2_mmHg"]:
        score -= pen["cabin_ppco2_mmHg"]
    elif ppco2 > limits["cabin_ppco2_mmHg_soft"]:
        score -= pen["cabin_ppco2_mmHg_soft"]
    if environment.get("spo2_pct", 100) < limits["spo2_pct"]:
        score -= pen["spo2_pct"]
    if environment.get("body_temperature_c", 0) > limits["body_temperature_c"]:
        score -= pen["body_temperature_c"]

    if isolation.get("sleep_hours_last_24h", 8) < limits["sleep_hours_last_24h"]:
        score -= pen["sleep_hours_last_24h"]
    if isolation.get("hrv_rmssd_drop_pct", 0) > limits["hrv_rmssd_drop_pct"]:
        score -= pen["hrv_rmssd_drop_pct"]
    if isolation.get("pvt_reaction_time_ms", 0) > limits["pvt_reaction_time_ms"]:
        score -= pen["pvt_reaction_time_ms"]
    if isolation.get("blink_rate_per_min", 99) < limits["blink_rate_per_min"]:
        score -= pen["blink_rate_per_min"]
    if isolation.get("microsleep_events", 0) >= limits["microsleep_events"]:
        score -= pen["microsleep_events"]

    return int(round(min(100.0, max(0.0, score))))


def load_studies(path: Path = DEFAULT_OSDR) -> list[dict]:
    if not path.exists():
        return []
    cache = json.loads(path.read_text(encoding="utf-8"))
    studies = []
    for page in cache.get("responses") or []:
        hits = ((page.get("body") or {}).get("hits") or {}).get("hits") or []
        for hit in hits:
            source = hit.get("_source") or {}
            accession = source.get("Accession") or source.get("Study Identifier")
            title = source.get("Study Title") or ""
            if accession and title:
                studies.append({
                    "accession": accession,
                    "title": title,
                    "description": source.get("Study Description") or "",
                })
    return studies


def cite(studies: list[dict], terms: list[str], limit: int = 2) -> list[dict]:
    if not terms:
        return []
    needles = [term.lower() for term in terms]
    found = []
    for study in studies:
        blob = f"{study['title']} {study['description']}".lower()
        if any(needle in blob for needle in needles):
            found.append({"accession": study["accession"], "title": study["title"]})
        if len(found) >= limit:
            break
    return found


def _format_value(value, unit: str) -> str:
    if isinstance(value, float) and not value.is_integer():
        text = f"{value:.1f}"
    else:
        text = str(int(value)) if isinstance(value, float) and value.is_integer() else str(value)
    return f"{text} {unit}".strip()


def assess(payload: dict, config: dict, studies: list[dict] | None = None) -> dict:
    limits = config["limits"]
    findings = []
    for rule in config["rules"]:
        if not rule_triggered(rule, payload, limits):
            continue
        if rule.get("match") == "any":
            sys_v = metric_value(payload, "blood_pressure_sys")
            dia_v = metric_value(payload, "blood_pressure_dia")
            value_text = f"{sys_v}/{dia_v} {rule['unit']}"
        else:
            value_text = _format_value(metric_value(payload, rule["metric"]), rule["unit"])
        findings.append({
            "id": rule["id"],
            "pillar": rule["pillar"],
            "severity": rule["severity"],
            "metric": rule.get("metric", rule["id"]),
            "value_text": value_text,
            "threshold_text": rule["threshold_text"],
            "diagnosis": rule["diagnosis"],
            "action": rule["action"],
            "citations": cite(studies or [], rule.get("osdr_terms") or []),
        })

    plan = {
        "agent": config["agent"]["id"],
        "agent_name": config["agent"]["name"],
        "agent_count": 1,
        "covers": config["agent"]["covers"],
        "health_score": health_score(payload, config),
        "findings": findings,
    }
    plan["text"] = render_plan(plan)
    plan["verified"], plan["rejected_numbers"] = verify_plan(plan["text"], payload, config, findings)
    return plan


def render_plan(plan: dict) -> str:
    lines = [
        f"{plan['agent_name']}. Un agente para {', '.join(plan['covers'])}.",
        f"Puntaje {plan['health_score']}.",
    ]
    if not plan["findings"]:
        lines.append("Ningún umbral de la tabla está vencido.")
    for finding in plan["findings"]:
        lines.append(
            f"{finding['severity']} {finding['metric']} {finding['value_text']} "
            f"(límite {finding['threshold_text']}). {finding['diagnosis']} "
            f"Acción: {finding['action']}"
        )
        for citation in finding["citations"]:
            lines.append(f"Fuente OSDR {citation['accession']}: {citation['title']}")
    return "\n".join(lines)


def _numbers(text: str) -> list[float]:
    cleaned = ACCESSION.sub(" ", text)
    return [float(token) for token in NUMBER.findall(cleaned)]


def _collect_numbers(value) -> list[float]:
    found = []
    if isinstance(value, bool):
        return found
    if isinstance(value, (int, float)):
        found.append(float(value))
    elif isinstance(value, str):
        found.extend(_numbers(value))
    elif isinstance(value, dict):
        for item in value.values():
            found.extend(_collect_numbers(item))
    elif isinstance(value, list):
        for item in value:
            found.extend(_collect_numbers(item))
    return found


def verify_plan(text: str, payload: dict, config: dict, findings: list[dict]) -> tuple[bool, list[float]]:
    allowed = _collect_numbers(payload) + _collect_numbers(config)
    allowed.append(float(health_score(payload, config)))
    for finding in findings:
        for citation in finding["citations"]:
            allowed.extend(_numbers(citation["title"]))
    rejected = []
    for number in _numbers(text):
        if not any(round(number, 2) == round(candidate, 2) for candidate in allowed):
            rejected.append(number)
    return (not rejected), rejected


def sample_payload() -> dict:
    return {
        "crew_member_id": "CMD-01",
        "survival_status": {
            "vital_signs_present": True,
            "sos_panic_button_active": False,
            "cabin_pressure_nominal": True,
        },
        "telemetry_ridge": {
            "R_radiation": {"career_accumulated_mSv": 145.2, "acute_event_mSv": 12.0},
            "I_isolation": {
                "sleep_hours_last_24h": 7.2,
                "hrv_rmssd_drop_pct": 12.0,
                "pvt_reaction_time_ms": 240,
                "blink_rate_per_min": 16,
                "microsleep_events": 0,
            },
            "D_distance": {"comm_delay_oneway_min": 20.5, "med_supply_days_remaining": 412},
            "G_gravity": {
                "heart_rate_bpm": 72,
                "blood_pressure_sys": 120,
                "blood_pressure_dia": 78,
                "onsd_mm": 4.8,
                "exercise_completed_hours": 1.5,
            },
            "E_environment": {
                "cabin_ppco2_mmHg": 2.1,
                "spo2_pct": 91,
                "body_temperature_c": 36.8,
            },
        },
    }


def _recv_exact(sock: socket.socket, size: int) -> bytes:
    chunks = []
    pending = size
    while pending:
        piece = sock.recv(pending)
        if not piece:
            raise ConnectionError("el socket se cerró")
        chunks.append(piece)
        pending -= len(piece)
    return b"".join(chunks)


def _read_text_frame(sock: socket.socket) -> str:
    header = _recv_exact(sock, 2)
    opcode = header[0] & 0x0F
    masked = (header[1] & 0x80) != 0
    length = header[1] & 0x7F
    if length == 126:
        length = struct.unpack("!H", _recv_exact(sock, 2))[0]
    elif length == 127:
        length = struct.unpack("!Q", _recv_exact(sock, 8))[0]
    mask = _recv_exact(sock, 4) if masked else b""
    payload = bytearray(_recv_exact(sock, length))
    if masked:
        for index, byte in enumerate(payload):
            payload[index] = byte ^ mask[index % 4]
    if opcode == 0x8:
        raise ConnectionError("el servidor cerró el socket")
    if opcode != 0x1:
        return ""
    return payload.decode("utf-8")


def listen(host: str, port: int, config: dict, studies: list[dict]) -> None:
    sock = socket.create_connection((host, port), timeout=10)
    sock.settimeout(None)
    key = base64.b64encode(b"0123456789abcdef").decode("ascii")
    request = (
        "GET / HTTP/1.1\r\n"
        f"Host: {host}:{port}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n\r\n"
    )
    sock.sendall(request.encode("ascii"))
    handshake = sock.recv(4096).decode("utf-8", errors="replace")
    if "101" not in handshake:
        raise ConnectionError(f"handshake rechazado: {handshake[:200]}")

    previous = None
    print(f"[MÉDICO DE VUELO] Escuchando ws://{host}:{port}", flush=True)
    try:
        while True:
            raw = _read_text_frame(sock)
            if not raw:
                continue
            try:
                message = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if not (message.get("crew_member_id") and message.get("telemetry_ridge")):
                continue
            plan = assess(message, config, studies)
            signature = (plan["health_score"], tuple(item["id"] for item in plan["findings"]))
            if signature == previous:
                continue
            previous = signature
            print(plan["text"], flush=True)
            if not plan["verified"]:
                print("Plan rechazado:", plan["rejected_numbers"], flush=True)
    finally:
        sock.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Médico de vuelo: un agente, una tabla de umbrales.")
    parser.add_argument("--config", type=Path, default=DEFAULT_CONFIG)
    parser.add_argument("--osdr", type=Path, default=DEFAULT_OSDR)
    parser.add_argument("--payload", type=Path, help="JSON de telemetría RIDGE. Sin esto, usa un caso de hipoxemia.")
    parser.add_argument("--listen", action="store_true", help="Escucha ws://127.0.0.1:9002 y emite un plan cuando cambia el cuadro.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9002)
    args = parser.parse_args()

    config = load_config(args.config)
    studies = load_studies(args.osdr)
    if args.listen:
        listen(args.host, args.port, config, studies)
        return

    payload = json.loads(args.payload.read_text(encoding="utf-8")) if args.payload else sample_payload()
    plan = assess(payload, config, studies)
    json.dump(plan, sys.stdout, ensure_ascii=False, indent=2)
    sys.stdout.write("\n")
    if not plan["verified"]:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
