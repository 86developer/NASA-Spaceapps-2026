# VitalGrid :: Deep Space Astronaut Health Monitoring & Autonomous Flight Surgeon

**NASA Space Apps Challenge — Deep Space Transit Biomedical Architecture (MVP)**  
*Mission Profile: Earth-Mars Cycler / Lunar Gateway / Artemis Long-Duration Missions*

---

## 1. Executive Summary & Aerospace System Architecture

During deep-space transit missions beyond low Earth orbit (LEO), communication latency to Earth ground control reaches between **3 to 22 minutes one-way** (up to 44 minutes round-trip light delay from Mars). In acute clinical emergencies or life-support failures, terrestrial telemedicine is rendered biologically obsolete.

**VitalGrid** is an aerospace-grade, decoupled autonomous health monitoring architecture designed to safeguard astronaut crews in deep space:

```
                                 +---------------------------------------------+
                                 |         NASA RIDGE TELEMETRY INGESTION      |
                                 |  [R]adiation  [I]solation  [D]istance       |
                                 |  [G]ravity (Cardio/SANS)   [E]nvironment    |
                                 +----------------------+----------------------+
                                                        |
                                                        v
+---------------------------------------------------------------------------------------------------------+
|                                    VITALGRID C++ BACKEND DAEMON                                         |
|                                                                                                         |
|   +------------------------------------+             +----------------------------------------------+   |
|   |   FLIGHT SURGEON AI ENGINE         |             |   SOMA WATCHDOG THREAD (High-Priority)       |   |
|   |   - NASA Clinical Rule Matrix      |             |   - Triggers: Asystole, SOS, Depressurization|   |
|   |   - Real-Time Threshold Evaluation |             |   - 1000 Hz Circular Ring Buffer Burst       |   |
|   |   - Tactical Action Directives     |             |   - Lossless Compression (Zstd L19)          |   |
|   +-----------------+------------------+             |   - Military Encryption (AES-256-GCM)        |   |
|                     |                                |   - DSOC 1550nm Laser Transmitter Queue      |   |
|                     v                                +----------------------+-----------------------+   |
|          [Clinical Alert Sub-JSON]                                          |                           |
|                     |                                                       v                           |
|                     +-----------------------> + <---------------------------+                           |
|                                               |                                                         |
|                                               v                                                         |
|                               +-------------------------------+                                         |
|                               |  RFC 6455 WEBSOCKET DAEMON    |                                         |
|                               |  Port 9002 (Lock-Free Fanout) |                                         |
|                               +---------------+---------------+                                         |
+-----------------------------------------------|---------------------------------------------------------+
                                                |  ws://127.0.0.1:9002
                                                v
                    +-------------------------------------------------------+
                    |             CREW DRAGON GLASS COCKPIT UI              |
                    |  - 1000 Hz Real-Time ECG Waveform Canvas              |
                    |  - Health Score Gauge & Survival Triad Indicators     |
                    |  - RIDGE Matrix (R, I, D, G, E) Telemetry Visualizer  |
                    |  - Autonomous Clinical Directives & Action Protocols  |
                    |  - Procedural Audio Synthesizer (Apollo/Artemis Tones)|
                    |  - Simulation Cockpit (Anomaly Injection Pad)         |
                    +-------------------------------------------------------+
```

---

## 2. NASA RIDGE 5-Pillar Data Model

The backend continuously ingests and validates structured JSON telemetry conforming to the 5 human spaceflight hazards identified by NASA's Human Research Program (HRP):

```json
{
  "timestamp": "2026-10-02T12:33:24Z",
  "crew_member_id": "CMD-01",
  "health_score": 92,
  "survival_status": {
    "vital_signs_present": true,
    "sos_panic_button_active": false,
    "cabin_pressure_nominal": true
  },
  "telemetry_ridge": {
    "R_radiation": {
      "career_accumulated_mSv": 145.2,
      "acute_event_mSv": 12.0
    },
    "I_isolation": {
      "sleep_hours_last_24h": 5.8,
      "hrv_rmssd_drop_pct": 18.5,
      "pvt_reaction_time_ms": 280
    },
    "D_distance": {
      "comm_delay_oneway_min": 20.5,
      "med_supply_days_remaining": 412
    },
    "G_gravity": {
      "heart_rate_bpm": 105,
      "blood_pressure_sys": 135,
      "blood_pressure_dia": 85,
      "onsd_mm": 5.2,
      "exercise_completed_hours": 1.5
    },
    "E_environment": {
      "cabin_ppco2_mmHg": 3.2,
      "spo2_pct": 94,
      "body_temperature_c": 37.9
    }
  }
}
```

---

## 3. Autonomous Decision Engine (Flight Surgeon AI)

The C++ `FlightSurgeonAI` engine evaluates telemetry against strict aerospace biomedical standards:

| Severity | Metric / Condition | Threshold Limit | Clinical Diagnosis | Immediate Tactical Resolution Action |
| :--- | :--- | :--- | :--- | :--- |
| **CRITICAL** | `acute_event_mSv` | `> 250.0 mSv` | Acute Radiation Syndrome (ARS) / Solar Particle Event (SPE) | Immediate evacuation to deep storm shelter core (water-shielded compartment). Administer Filgrastim (G-CSF) 300mcg SQ. Power down unshielded avionics. |
| **CRITICAL** | `career_accumulated_mSv` | `> 600.0 mSv` | Cumulative Lifetime Radiation Ceiling Exceeded | Terminate EVA operations. Reassign to high-shielded habitat watch. Initiate daily lymphocyte chromosomal assays. |
| **CRITICAL** | `blood_pressure` | `> 155/95 mmHg` | Hypertensive Crisis Under Microgravity | Cease resistive exercises. Secure in supine restraint. Administer sublingual Captopril 25mg; monitor every 10 min. |
| **CRITICAL** | `heart_rate_bpm` | `> 110 bpm` | Sustained Supraventricular Tachycardia | Perform vagal maneuvers (modified Valsalva). Verify electrolyte levels (K+/Mg2+). IV Metoprolol 5mg if persistent. |
| **CRITICAL** | `onsd_mm` | `> 5.80 mm` | SANS — Elevated Intracranial Pressure (ICP) | Deploy Lower Body Negative Pressure (LBNP) device at -35 mmHg for 2 hours. Administer oral Acetazolamide 250mg. Schedule OCT scan. |
| **CRITICAL** | `cabin_ppco2_mmHg` | `> 3.00 mmHg` | Atmospheric Hypercapnia / CO2 Retention | Cycle CDRA to Fast Desorption mode. Don emergency crew O2 masks. Activate LiOH backup chemical scrubber canisters. |
| **CRITICAL** | `spo2_pct` | `< 95%` | Acute Systemic Hypoxemia | Deliver 100% O2 via aviator mask at 6 L/min. Inspect cabin/suit ppO2 sensors. Auscultate lung bases for atelectasis. |
| **CRITICAL** | `body_temperature_c` | `> 38.5 °C` | Space Pyrexia / Thermoregulatory Collapse or Sepsis | Engage Liquid Cooling Garment (LCVG) active chiller loop. Infuse IV Paracetamol 1000mg. Run blood Bio-Analyzer cartridge. |
| **WARNING** | `sleep_hours_last_24h` | `< 6.0 hours` | Chronic Sleep Debt & Cognitive Fatigue | Prescribe mandatory 60-min restorative rest. Shift ambient lighting to 480nm-depleted warm circadian spectrum. Hold robotics tasks. |
| **WARNING** | `hrv_rmssd_drop_pct` | `> 25.0%` | Autonomic Sympathetic Exhaustion | Guided biofeedback resonance breathing (0.1 Hz pacing) for 15 min. Reduce sensory workload. Assess cortisol biomarkers. |

When any threshold is breached, the engine generates and broadcasts an actionable sub-JSON alert:

```json
{
  "type": "CLINICAL_ALERT",
  "status": "CRITICAL",
  "trigger_metric": "cabin_ppco2_mmHg",
  "trigger_value": "3.20 mmHg",
  "threshold_limit": "> 3.0 mmHg",
  "diagnosis": "CABIN ATMOSPHERIC HYPERCAPNIA - CO2 RETENTION / ECLSS DEGRADATION",
  "tactical_action": "Cycle Carbon Dioxide Removal Assembly (CDRA) beds to Fast Desorption mode. Don emergency crew O2 masks. Activate LiOH backup chemical scrubber cartridges.",
  "timestamp": "2026-10-02T12:33:24Z"
}
```

---

## 4. SOMA Module (Spacecraft Offline Mortality & Anomaly Archive)

The **SOMA** module operates on a dedicated high-priority watchdog thread.

### Emergency Trigger Condition:
$$\text{Trigger} = (\neg \text{vital\_signs\_present}) \lor (\text{sos\_panic\_button\_active}) \lor (\neg \text{cabin\_pressure\_nominal})$$

### "Science over Survival" Execution Sequence:
1. **Sampling Frequency Boost**: The telemetry ring buffer increases sampling from nominal 1 Hz to **1000 Hz** (1-millisecond resolution burst capture).
2. **Container Packaging**: Historical RIDGE snapshots are extracted and wrapped into an aerospace black-box flight recorder archive.
3. **Lossless Compression**: Compressed using simulated Zstandard (Zstd level 19 dictionary), achieving typical ratios of **3.65x**.
4. **Military-Grade Encryption**: Secured with authenticated **AES-256-GCM** (NIST SP 800-38D), producing a 96-bit initialization vector (`iv_nonce_hex`) and 128-bit authentication tag (`auth_tag_hex`).
5. **DSOC Optical Laser Uplink**: Payload is enqueued into the **Deep Space Optical Communications (DSOC)** 1550nm flight laser terminal at 267 Mbps, aimed at Earth DSN ground stations (Goldstone / Palomar).
6. **Prominent Console Broadcast**:
```
================================================================================
  [SOMA DSOC BURST TRANSMITTED]
  TIMESTAMP:         2026-10-02T12:34:00Z
  CREW ID:           CMD-01
  TRIGGER REASON:    RAPID CABIN DEPRESSURIZATION / ECLSS STRUCTURAL BREACH
  TELEMETRY SAMPLES: 500 packets (1000 Hz burst archive)
  DATA SIZE:         68400 B raw -> 18739 B (Zstd 3.65x)
  SECURITY CIPHER:   AES-256-GCM (Nonce: 0x9F3E8B1A4C2D... | Tag: 0x5C81E3FA...)
  TRANSMITTER:       DSOC 1550nm Uplink Laser -> NASA DSN Goldstone 34m Beam Waveguide
  ONE-WAY DELAY:     20.5 min light-speed delay to Earth
================================================================================
```

---

## 5. Directory Structure

```
f:\VitalGrid\
├── CMakeLists.txt              # Standard CMake build configuration
├── Makefile                    # Cross-platform Makefile (Windows / Linux / macOS)
├── build.bat                   # 1-click Windows compilation script
├── run.bat                     # 1-click launch script (builds, opens UI & starts daemon)
├── vitalgrid_daemon.exe        # Pre-compiled high-performance binary
├── src\
│   ├── main.cpp                # Daemon lifecycle, telemetry ingestion loop & signal handling
│   ├── models\
│   │   ├── ridge_payload.hpp   # RIDGE 5-pillar structs & exact JSON serialization
│   │   └── alert_payload.hpp   # Clinical alert & tactical directives model
│   ├── engine\
│   │   ├── flight_surgeon.hpp  # NASA clinical thresholds specification
│   │   └── flight_surgeon.cpp  # Autonomous rule evaluation & health scoring logic
│   ├── soma\
│   │   ├── circular_buffer.hpp # Thread-safe 1000Hz circular ring buffer
│   │   ├── soma_watchdog.hpp   # SOMA watchdog thread & DSOC transmission records
│   │   └── soma_watchdog.cpp   # Science-over-survival burst sequence implementation
│   └── network\
│       ├── websocket_server.hpp# Self-contained RFC-6455 WebSocket server header
│       └── websocket_server.cpp# WinSock2/POSIX implementation with built-in SHA1/Base64
└── web\
    ├── index.html              # Crew Dragon glass-cockpit interactive dashboard
    ├── css\
    │   └── style.css           # Futuristic aerospace HUD design system
    └── js\
        ├── audio_synth.js      # Procedural Web Audio API sound synthesizer
        └── app.js              # WebSocket telemetry handler, live ECG & anomaly pad
```
---

## 6. 🚀 Guía Rápida para el Equipo (Cómo Correr el Proyecto en 2 Minutos)

¡Bienvenido al equipo de **VitalGrid**! Sigue estos pasos para clonar y levantar el sistema de monitoreo biomédico en tu computadora:

### Paso 1: Clonar el Repositorio
Abre tu terminal (PowerShell, CMD, Git Bash o terminal de Linux/macOS) y clona el proyecto:
```bash
git clone https://github.com/86developer/NASA-Spaceapps-2026.git
cd NASA-Spaceapps-2026
```

---

### Paso 2: Levantar el Proyecto

#### 🪟 En Windows (El método más fácil - 1 Clic):
Simplemente ejecuta el script automatizado:
```cmd
.\run.bat
```
> **¿Qué hace `run.bat` automáticamente?**
> 1. Detecta tu compilador `g++` (incluso si está instalado vía Scoop o MinGW).
> 2. Compila el demonio `vitalgrid_daemon.exe` optimizado con `-O3`.
> 3. Abre automáticamente la interfaz **Crew Dragon** (`web/index.html`) en tu navegador predeterminado.
> 4. Inicia el servidor WebSocket en segundo plano en `ws://127.0.0.1:9002`.

#### 🐧 En Linux / macOS / WSL:
Compila con `g++` (o `clang++`) y levanta el demonio:
```bash
# Compilar:
g++ -std=c++17 -O3 -Wall -Wextra src/main.cpp src/engine/flight_surgeon.cpp src/soma/soma_watchdog.cpp src/network/websocket_server.cpp -pthread -o vitalgrid_daemon

# Iniciar backend:
./vitalgrid_daemon 9002
```
Luego abre el archivo `web/index.html` en tu navegador favorito (Chrome, Firefox, Edge, Safari).

---

### 🛠️ ¿No tienes compilador C++ instalado?
- **En Windows (Recomendado vía Scoop):**
  ```powershell
  scoop install gcc
  ```
- **En Ubuntu / Debian:**
  ```bash
  sudo apt update && sudo apt install -y build-essential
  ```
- **En macOS:**
  ```bash
  xcode-select --install
  ```

---

### Paso 3: Interactuar con el Dashboard
Una vez abierto el dashboard:
1. Verás el estado **"TELEMETRY ONLINE"** en verde en la barra superior.
2. El ECG cardíaco comenzará a dibujar en tiempo real a 1000 Hz sincronizado con la frecuencia cardíaca.
3. En la columna derecha encontrarás el **SIMULATION COCKPIT (ANOMALY INJECTION PAD)**:
   - Haz clic en **`🚨 TRIGGER SOS BUTTON`**, **`💥 DEPRESSURIZE CABIN`** o **`💔 ASYSTOLE / COLLAPSE`** para activar el protocolo de emergencia **SOMA** ("Ciencia sobre Supervivencia"), ver el empaquetado forense Zstd, el cifrado militar AES-256-GCM y la transmisión láser DSOC a 1550nm hacia la Tierra.
   - Haz clic en **`🫁 INJECT HYPOXIA`**, **`💨 INJECT ppCO2`** o **`👁️ INJECT SANS`** para ver cómo la IA del **Flight Surgeon** emite directivas tácticas inmediatas.
   - Haz clic en **`✨ RESTORE ALL TO NOMINAL BASELINE`** para reiniciar todos los parámetros al estado de salud ideal.

---

## 7. Opciones Avanzadas de Compilación (CMake / Make)

### Con CMake:
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
./vitalgrid_daemon 9002
```

### Con Makefile:
```bash
make
./vitalgrid_daemon.exe
```

---

## 8. Ejecución de Pruebas de Integración (Opcional)
Para verificar la conexión de telemetría y el disparo de SOMA desde scripts automatizados:
```bash
python tests/test_stream.py
python tests/test_soma.py
```

