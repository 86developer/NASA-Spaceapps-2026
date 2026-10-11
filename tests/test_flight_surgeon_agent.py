import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "agents"))

import flight_surgeon as surgeon


def load():
    config = surgeon.load_config()
    studies = surgeon.load_studies()
    return config, studies


def test_un_solo_agente():
    config, _ = load()
    assert config["agent"]["count"] == 1
    assert config["agent"]["id"] == "flight_surgeon"
    assert config["agent"]["covers"] == ["R", "I", "D", "G", "E"]
    assert "agents" not in config


def test_hipoxemia_y_puntaje():
    config, studies = load()
    plan = surgeon.assess(surgeon.sample_payload(), config, studies)
    assert [item["id"] for item in plan["findings"]] == ["hypoxemia"]
    assert plan["health_score"] == 70
    assert plan["verified"] is True
    assert plan["rejected_numbers"] == []
    assert "91 %" in plan["text"]
    assert plan["agent_count"] == 1


def test_radiacion_cita_un_estudio_real():
    config, studies = load()
    payload = surgeon.sample_payload()
    payload["telemetry_ridge"]["R_radiation"]["acute_event_mSv"] = 300
    payload["telemetry_ridge"]["E_environment"]["spo2_pct"] = 98
    payload["telemetry_ridge"]["E_environment"]["cabin_ppco2_mmHg"] = 1.5
    plan = surgeon.assess(payload, config, studies)
    assert [item["id"] for item in plan["findings"]] == ["acute_radiation"]
    citations = plan["findings"][0]["citations"]
    assert citations
    by_accession = {study["accession"]: study for study in studies}
    for citation in citations:
        study = by_accession[citation["accession"]]
        blob = f"{study['title']} {study['description']}".lower()
        assert "radiation" in blob
    assert plan["verified"] is True


def test_un_numero_inventado_se_rechaza():
    config, studies = load()
    plan = surgeon.assess(surgeon.sample_payload(), config, studies)
    tampered = plan["text"] + "\nDosis inventada 9999 mg."
    verified, rejected = surgeon.verify_plan(tampered, surgeon.sample_payload(), config, plan["findings"])
    assert verified is False
    assert 9999 in rejected


def test_asistolia_anula_el_puntaje():
    config, _ = load()
    payload = surgeon.sample_payload()
    payload["survival_status"]["vital_signs_present"] = False
    assert surgeon.health_score(payload, config) == 0


if __name__ == "__main__":
    test_un_solo_agente()
    test_hipoxemia_y_puntaje()
    test_radiacion_cita_un_estudio_real()
    test_un_numero_inventado_se_rechaza()
    test_asistolia_anula_el_puntaje()
    print("flight surgeon agent ok")
    print(json.dumps({"agent": "flight_surgeon", "count": 1}, ensure_ascii=False))
