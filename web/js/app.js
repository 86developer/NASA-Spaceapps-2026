/**
 * VitalGrid - Crew Dragon Aerospace HUD & Telemetry Controller
 * NASA Space Apps Challenge - Deep Space Astronaut Health Monitoring
 */

document.addEventListener('DOMContentLoaded', () => {
    // --- State Variables ---
    let ws = null;
    let wsUrl = 'ws://127.0.0.1:9002';
    let reconnectTimer = null;
    let activeAlerts = new Map();
    let currentPayload = null;
    let somaTriggered = false;

    // --- DOM Elements ---
    const connectionPill = document.getElementById('connectionPill');
    const connectionLabel = document.getElementById('connectionLabel');
    const somaPill = document.getElementById('somaPill');
    const somaLabel = document.getElementById('somaLabel');
    const muteBtn = document.getElementById('muteBtn');
    const somaStrip = document.getElementById('somaStrip');
    const somaDetailsCard = document.getElementById('somaDetailsCard');
    const metClock = document.getElementById('metClock');
    const utcClock = document.getElementById('utcClock');
    const rawJsonViewer = document.getElementById('rawJsonViewer');
    const directivesList = document.getElementById('directivesList');

    // Health & Survival
    const healthScoreNum = document.getElementById('healthScoreNum');
    const healthGaugeProgress = document.getElementById('healthGaugeProgress');
    const crewIdEl = document.getElementById('crewId');
    const vitalSignsPill = document.getElementById('vitalSignsPill');
    const sosPill = document.getElementById('sosPill');
    const cabinPressurePill = document.getElementById('cabinPressurePill');

    // Canvas ECG
    const ecgCanvas = document.getElementById('ecgCanvas');
    const ecgCtx = ecgCanvas.getContext('2d');
    const ecgBpmDisplay = document.getElementById('ecgBpmDisplay');

    // --- Audio Init on user click ---
    document.body.addEventListener('click', () => {
        window.aerospaceAudio.init();
    }, { once: true });

    muteBtn.addEventListener('click', () => {
        const isMuted = window.aerospaceAudio.toggleMute();
        muteBtn.textContent = isMuted ? 'UNMUTE AUDIO' : 'MUTE AUDIO';
        muteBtn.style.color = isMuted ? 'var(--text-muted)' : 'var(--hud-cyan)';
    });

    // --- Mission Elapsed Time (MET) Clock ---
    let metSeconds = 42 * 86400 + 18 * 3600 + 24 * 60 + 12;
    setInterval(() => {
        metSeconds++;
        const d = Math.floor(metSeconds / 86400);
        const h = Math.floor((metSeconds % 86400) / 3600);
        const m = Math.floor((metSeconds % 3600) / 60);
        const s = metSeconds % 60;
        metClock.textContent = `MET +${String(d).padStart(3, '0')}:${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}:${String(s).padStart(2, '0')}`;
        utcClock.textContent = new Date().toISOString().substring(11, 19) + ' UTC';
    }, 1000);

    // --- Real-time ECG Canvas Generator ---
    let ecgPoints = [];
    let ecgX = 0;
    let currentBpm = 105;
    let isVitalsPresent = true;

    function resizeCanvas() {
        if (!ecgCanvas) return;
        ecgCanvas.width = ecgCanvas.clientWidth * window.devicePixelRatio;
        ecgCanvas.height = ecgCanvas.clientHeight * window.devicePixelRatio;
        ecgCtx.scale(window.devicePixelRatio, window.devicePixelRatio);
    }
    window.addEventListener('resize', resizeCanvas);
    resizeCanvas();

    function drawECG() {
        const w = ecgCanvas.clientWidth;
        const h = ecgCanvas.clientHeight;
        const midY = h / 2;

        ecgCtx.fillStyle = 'rgba(4, 7, 11, 0.2)';
        ecgCtx.fillRect(0, 0, w, h);

        // Draw grid
        ecgCtx.strokeStyle = 'rgba(0, 243, 255, 0.05)';
        ecgCtx.lineWidth = 1;
        for (let gx = 0; gx < w; gx += 20) {
            ecgCtx.beginPath();
            ecgCtx.moveTo(gx, 0);
            ecgCtx.lineTo(gx, h);
            ecgCtx.stroke();
        }
        for (let gy = 0; gy < h; gy += 20) {
            ecgCtx.beginPath();
            ecgCtx.moveTo(0, gy);
            ecgCtx.lineTo(w, gy);
            ecgCtx.stroke();
        }

        // Generate next ECG point
        let sample = 0;
        if (!isVitalsPresent) {
            // Asystole flatline with subtle sensor noise
            sample = (Math.random() - 0.5) * 1.5;
        } else {
            const beatPeriod = Math.max(30, Math.floor(3600 / currentBpm));
            const phase = ecgX % beatPeriod;

            if (phase >= 10 && phase < 14) {
                // P wave
                sample = Math.sin((phase - 10) / 4 * Math.PI) * 7;
            } else if (phase === 20) {
                // Q drop
                sample = -10;
            } else if (phase === 22) {
                // R peak spike
                sample = 42;
            } else if (phase === 24) {
                // S drop
                sample = -16;
            } else if (phase >= 32 && phase < 42) {
                // T wave
                sample = Math.sin((phase - 32) / 10 * Math.PI) * 12;
            } else {
                sample = (Math.random() - 0.5) * 2;
            }
        }

        const pointY = midY - sample;
        ecgPoints.push({ x: ecgX % w, y: pointY });
        if (ecgPoints.length > w) {
            ecgPoints.shift();
        }

        // Draw trace
        ecgCtx.beginPath();
        ecgCtx.strokeStyle = isVitalsPresent ? '#00f3ff' : '#ff0055';
        ecgCtx.lineWidth = 2;
        ecgCtx.shadowColor = isVitalsPresent ? 'rgba(0, 243, 255, 0.8)' : 'rgba(255, 0, 85, 0.9)';
        ecgCtx.shadowBlur = 8;

        const currentPos = ecgX % w;
        for (let i = 1; i < ecgPoints.length; i++) {
            const p0 = ecgPoints[i - 1];
            const p1 = ecgPoints[i];
            if (Math.abs(p1.x - p0.x) < 5) {
                ecgCtx.moveTo(p0.x, p0.y);
                ecgCtx.lineTo(p1.x, p1.y);
            }
        }
        ecgCtx.stroke();
        ecgCtx.shadowBlur = 0;

        // Clear beam head
        ecgCtx.clearRect((currentPos + 1) % w, 0, 16, h);

        ecgX++;
        requestAnimationFrame(drawECG);
    }
    requestAnimationFrame(drawECG);

    // --- WebSocket Client Implementation ---
    function connectWebSocket() {
        console.log(`[VitalGrid UI] Connecting to ${wsUrl}...`);
        connectionLabel.textContent = 'CONNECTING...';
        connectionPill.className = 'status-pill disconnected';

        try {
            ws = new WebSocket(wsUrl);
        } catch (e) {
            console.error('[WS ERROR]', e);
            scheduleReconnect();
            return;
        }

        ws.onopen = () => {
            console.log('[VitalGrid UI] Connected to Flight Surgeon C++ Daemon');
            connectionLabel.textContent = 'TELEMETRY ONLINE';
            connectionPill.className = 'status-pill connected';
            if (reconnectTimer) clearTimeout(reconnectTimer);
        };

        ws.onmessage = (event) => {
            try {
                const data = JSON.parse(event.data);
                handleIncomingMessage(data, event.data);
            } catch (err) {
                console.error('[JSON PARSE ERROR]', err, event.data);
            }
        };

        ws.onclose = () => {
            console.warn('[VitalGrid UI] Connection closed');
            connectionLabel.textContent = 'DISCONNECTED (RETRYING)';
            connectionPill.className = 'status-pill disconnected';
            scheduleReconnect();
        };

        ws.onerror = (err) => {
            console.error('[WS SOCKET ERROR]', err);
            ws.close();
        };
    }

    function scheduleReconnect() {
        if (reconnectTimer) clearTimeout(reconnectTimer);
        reconnectTimer = setTimeout(connectWebSocket, 2000);
    }

    // --- Message Dispatcher ---
    function handleIncomingMessage(msg, rawString) {
        if (msg.type === 'SOMA_DSOC_EVENT') {
            handleSomaEvent(msg);
            return;
        }

        if (msg.type === 'CLINICAL_ALERT') {
            handleClinicalAlert(msg);
            return;
        }

        if (msg.crew_member_id && msg.telemetry_ridge) {
            handleRidgeTelemetry(msg, rawString);
        }
    }

    // --- Ingest RIDGE Telemetry ---
    function handleRidgeTelemetry(data, raw) {
        currentPayload = data;
        rawJsonViewer.textContent = JSON.stringify(data, null, 2);

        crewIdEl.textContent = data.crew_member_id || 'CMD-01';

        // Health Score
        const score = data.health_score ?? 100;
        healthScoreNum.textContent = score;
        
        // Gauge ring progress (circumference = 2 * PI * 70 = 440)
        const offset = 440 - (score / 100) * 440;
        healthGaugeProgress.style.strokeDashoffset = offset;

        if (score >= 80) {
            healthGaugeProgress.style.stroke = 'var(--bio-green)';
            healthScoreNum.style.color = '#fff';
        } else if (score >= 50) {
            healthGaugeProgress.style.stroke = 'var(--warn-amber)';
            healthScoreNum.style.color = 'var(--warn-amber)';
        } else {
            healthGaugeProgress.style.stroke = 'var(--crit-crimson)';
            healthScoreNum.style.color = 'var(--crit-crimson)';
        }

        // Survival Triad
        const surv = data.survival_status;
        isVitalsPresent = surv.vital_signs_present;

        updateSurvivalPill(vitalSignsPill, surv.vital_signs_present, 'DETECTED (ACTIVE)', 'ASYSTOLE / COLLAPSE');
        updateSurvivalPill(sosPill, !surv.sos_panic_button_active, 'ARMED (STANDBY)', 'ENGAGED / ACTIVE');
        updateSurvivalPill(cabinPressurePill, surv.cabin_pressure_nominal, '101.3 kPa NOMINAL', 'DEPRESSURIZATION!');

        // Update RIDGE metrics
        const r = data.telemetry_ridge.R_radiation;
        const i = data.telemetry_ridge.I_isolation;
        const d = data.telemetry_ridge.D_distance;
        const g = data.telemetry_ridge.G_gravity;
        const e = data.telemetry_ridge.E_environment;

        currentBpm = g.heart_rate_bpm;
        ecgBpmDisplay.textContent = `${currentBpm} BPM`;

        // R - Radiation
        setMetric('val_rad_acute', r.acute_event_mSv.toFixed(1), r.acute_event_mSv > 250);
        setMetric('val_rad_career', r.career_accumulated_mSv.toFixed(1), r.career_accumulated_mSv > 600);
        const careerPct = Math.min(100, (r.career_accumulated_mSv / 600) * 100);
        const careerBar = document.getElementById('careerRadBar');
        if (careerBar) {
            careerBar.style.width = `${careerPct}%`;
            careerBar.className = 'progress-bar-fill ' + (careerPct > 80 ? 'crit' : careerPct > 50 ? 'warn' : '');
        }

        // I - Isolation
        setMetric('val_sleep_hours', i.sleep_hours_last_24h.toFixed(1), false, i.sleep_hours_last_24h < 6.0);
        setMetric('val_hrv_drop', i.hrv_rmssd_drop_pct.toFixed(1), false, i.hrv_rmssd_drop_pct > 25.0);
        setMetric('val_pvt_ms', i.pvt_reaction_time_ms, false, i.pvt_reaction_time_ms > 350);

        // D - Distance
        setMetric('val_comm_delay', d.comm_delay_oneway_min.toFixed(1), false);
        setMetric('val_med_supply', d.med_supply_days_remaining, false);

        // G - Gravity / Cardio / SANS
        setMetric('val_hr', g.heart_rate_bpm, g.heart_rate_bpm > 110 || g.heart_rate_bpm === 0);
        const bpText = `${g.blood_pressure_sys}/${g.blood_pressure_dia}`;
        setMetric('val_bp', bpText, g.blood_pressure_sys > 155 || g.blood_pressure_dia > 95);
        setMetric('val_onsd', g.onsd_mm.toFixed(2), g.onsd_mm > 5.8);
        setMetric('val_exercise', g.exercise_completed_hours.toFixed(1), false);

        // E - Environment
        setMetric('val_ppco2', e.cabin_ppco2_mmHg.toFixed(2), e.cabin_ppco2_mmHg > 3.0);
        setMetric('val_spo2', e.spo2_pct, e.spo2_pct < 95);
        setMetric('val_temp', e.body_temperature_c.toFixed(1), e.body_temperature_c > 38.5);

        // Check if survival is restored and reset SOMA indicator if needed
        if (surv.vital_signs_present && !surv.sos_panic_button_active && surv.cabin_pressure_nominal) {
            if (somaTriggered) {
                // SOMA cleared
                somaTriggered = false;
                somaPill.className = 'status-pill soma-standby';
                somaLabel.textContent = 'SOMA WATCHDOG: NOMINAL';
                somaStrip.classList.remove('visible');
                somaDetailsCard.classList.remove('visible');
                window.aerospaceAudio.stopAlarm();
            }
        }
    }

    function setMetric(elemId, value, isCrit, isWarn = false) {
        const el = document.getElementById(elemId);
        if (!el) return;
        el.textContent = value;
        const cell = el.closest('.metric-cell');
        if (cell) {
            cell.classList.remove('critical-breach', 'warning-breach');
            if (isCrit) {
                cell.classList.add('critical-breach');
            } else if (isWarn) {
                cell.classList.add('warning-breach');
            }
        }
    }

    function updateSurvivalPill(element, isNominal, nominalText, dangerText) {
        if (!element) return;
        element.className = `survival-pill ${isNominal ? 'nominal' : 'danger'}`;
        const tag = element.querySelector('.tag');
        if (tag) {
            tag.textContent = isNominal ? nominalText : dangerText;
        }
    }

    // --- Ingest Clinical Alert sub-JSON ---
    function handleClinicalAlert(alert) {
        const key = alert.trigger_metric;
        activeAlerts.set(key, { ...alert, receivedAt: new Date() });
        renderDirectives();

        if (alert.status === 'CRITICAL') {
            window.aerospaceAudio.startMasterAlarm();
        } else if (alert.status === 'WARNING') {
            window.aerospaceAudio.playCautionChime();
        }
    }

    function renderDirectives() {
        directivesList.innerHTML = '';

        if (activeAlerts.size === 0) {
            directivesList.innerHTML = `
                <div class="directive-card NOMINAL">
                    <div class="directive-header">
                        <span class="directive-status-tag">NOMINAL</span>
                        <span class="directive-time">AUTONOMOUS FLIGHT SURGEON ACTIVE</span>
                    </div>
                    <div class="directive-diagnosis">ALL BIOMEDICAL STANDARDS WITHIN NASA LIMITS</div>
                    <div class="directive-trigger">Continuous passive monitoring active across 5 RIDGE pillars.</div>
                    <div class="directive-action-box">
                        <div class="directive-action-label">Directive:</div>
                        Maintain scheduled deep space operational duties and countermeasure exercises.
                    </div>
                </div>
            `;
            return;
        }

        // Render sorted by severity
        const sorted = Array.from(activeAlerts.values()).sort((a, b) => {
            if (a.status === 'CRITICAL' && b.status !== 'CRITICAL') return -1;
            if (b.status === 'CRITICAL' && a.status !== 'CRITICAL') return 1;
            return 0;
        });

        sorted.forEach(alert => {
            const card = document.createElement('div');
            card.className = `directive-card ${alert.status}`;
            card.innerHTML = `
                <div class="directive-header">
                    <span class="directive-status-tag">${alert.status}</span>
                    <span class="directive-time">${alert.timestamp || new Date().toISOString().substring(11, 19) + ' UTC'}</span>
                </div>
                <div class="directive-diagnosis">${alert.diagnosis}</div>
                <div class="directive-trigger">Trigger: ${alert.trigger_metric} = ${alert.trigger_value} (${alert.threshold_limit})</div>
                <div class="directive-action-box">
                    <div class="directive-action-label">TACTICAL RESOLUTION ACTION:</div>
                    ${alert.tactical_action}
                </div>
            `;
            directivesList.appendChild(card);
        });
    }

    // --- Ingest SOMA DSOC Emergency Archive Event ---
    function handleSomaEvent(soma) {
        console.warn('[SOMA EMERGENCY BURST INGESTED]', soma);
        somaTriggered = true;

        somaPill.className = 'status-pill soma-active';
        somaLabel.textContent = 'SOMA: SCIENCE OVER SURVIVAL';
        somaStrip.classList.add('visible');

        document.getElementById('somaReasonText').textContent = soma.trigger_reason;
        document.getElementById('somaDsocStation').textContent = soma.target_ground_station;
        document.getElementById('somaDelayText').textContent = `${soma.light_delay_minutes} min light delay`;

        // Fill SOMA details card
        somaDetailsCard.classList.add('visible');
        document.getElementById('somaSamplesCount').textContent = `${soma.historical_samples_count} records (1000 Hz)`;
        document.getElementById('somaDataSize').textContent = `${soma.raw_payload_bytes} B raw → ${soma.compressed_payload_bytes} B`;
        document.getElementById('somaCompression').textContent = `${soma.compression_algorithm} (Ratio: ${soma.compression_ratio}x)`;
        document.getElementById('somaCipher').textContent = soma.encryption_cipher;
        document.getElementById('somaNonce').textContent = soma.iv_nonce_hex;
        document.getElementById('somaTag').textContent = soma.auth_tag_hex;

        window.aerospaceAudio.playDsocBurstLaser();
        window.aerospaceAudio.startMasterAlarm();
    }

    // --- Interactive Command Cockpit (Simulation Ingestion) ---
    function sendCommand(cmd) {
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            alert('WebSocket connection is not established. Please make sure vitalgrid_daemon.exe is running on port 9002.');
            return;
        }
        ws.send(cmd);
        console.log(`[VITALGRID COMMAND SENT] ${cmd}`);
    }

    // Bind simulation buttons
    document.getElementById('btnSos')?.addEventListener('click', () => sendCommand('TRIGGER_SOS'));
    document.getElementById('btnDepressurize')?.addEventListener('click', () => sendCommand('DEPRESSURIZE'));
    document.getElementById('btnAsystole')?.addEventListener('click', () => sendCommand('ASYSTOLE'));
    document.getElementById('btnHypoxia')?.addEventListener('click', () => sendCommand('INJECT_HYPOXIA'));
    document.getElementById('btnHypercapnia')?.addEventListener('click', () => sendCommand('INJECT_HYPERCAPNIA'));
    document.getElementById('btnSans')?.addEventListener('click', () => sendCommand('INJECT_SANS'));
    document.getElementById('btnSpeRadiation')?.addEventListener('click', () => sendCommand('INJECT_SPE_RADIATION'));
    document.getElementById('btnFever')?.addEventListener('click', () => sendCommand('INJECT_FEVER'));
    document.getElementById('btnFatigue')?.addEventListener('click', () => sendCommand('INJECT_FATIGUE'));
    
    document.getElementById('btnResetNominal')?.addEventListener('click', () => {
        activeAlerts.clear();
        renderDirectives();
        window.aerospaceAudio.stopAlarm();
        sendCommand('RESET_NOMINAL');
    });

    document.getElementById('btnResetSurvival')?.addEventListener('click', () => {
        window.aerospaceAudio.stopAlarm();
        sendCommand('RESET_SURVIVAL');
    });

    // Start WebSocket
    connectWebSocket();
});
