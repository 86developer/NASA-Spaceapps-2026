/**
 * Panel de cámara para la comparación de hinchazón facial.
 * El video vive solo en el elemento <video>. Al cortar la cámara se sueltan
 * los tracks y no queda ningún frame. localStorage recibe tres índices.
 */
(function () {
    'use strict';

    var VISION_URL = 'https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14/+esm';
    var WASM_URL = 'https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14/wasm';
    var MODEL_URL = 'https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task';

    document.addEventListener('DOMContentLoaded', function () {
        var edema = window.FacialEdema;
        var video = document.getElementById('edemaVideo');
        var idle = document.getElementById('edemaIdle');
        var deltaEl = document.getElementById('edemaDelta');
        var deltaLabel = document.getElementById('edemaDeltaLabel');
        var cheekEl = document.getElementById('edemaCheek');
        var jawEl = document.getElementById('edemaJaw');
        var roundEl = document.getElementById('edemaRound');
        var baselineEl = document.getElementById('edemaBaselineDate');
        var daysEl = document.getElementById('edemaDays');
        var chartEl = document.getElementById('edemaChart');
        var historyEl = document.getElementById('edemaHistory');
        var statusEl = document.getElementById('edemaStatus');
        var cameraBtn = document.getElementById('edemaCameraBtn');
        var captureBtn = document.getElementById('edemaCaptureBtn');
        var resetBtn = document.getElementById('edemaResetBtn');
        if (!edema || !video || !cameraBtn) return;

        try {
            edema.selfTest();
        } catch (err) {
            statusEl.textContent = 'El cálculo de hinchazón falló su prueba interna: ' + err.message;
            cameraBtn.disabled = true;
            return;
        }

        var store = edema.loadStore();
        var buffer = [];
        var stream = null;
        var landmarker = null;
        var running = false;
        var raf = 0;
        var starting = false;

        function formatPct(value) {
            var sign = value > 0 ? '+' : '';
            return sign + value.toFixed(1) + '%';
        }

        function setStatus(text) {
            statusEl.textContent = text;
        }

        function paintReadout(sample, label) {
            if (!store.baseline || !sample) {
                deltaEl.textContent = store.baseline ? '0.0%' : '—';
                deltaEl.className = 'edema-delta';
                deltaLabel.textContent = label;
                cheekEl.textContent = '—';
                jawEl.textContent = '—';
                roundEl.textContent = '—';
                return;
            }
            var deltas = edema.componentDeltas(sample, store.baseline);
            var drift = edema.meanDelta(deltas);
            deltaEl.textContent = formatPct(drift);
            deltaEl.className = 'edema-delta ' + edema.band(drift);
            deltaLabel.textContent = label;
            cheekEl.textContent = formatPct(deltas.cheek);
            jawEl.textContent = formatPct(deltas.jaw);
            roundEl.textContent = formatPct(deltas.roundness);
        }

        function renderStore() {
            if (!store.baseline) {
                baselineEl.textContent = 'Día 1: sin fijar';
                daysEl.textContent = '';
                captureBtn.textContent = 'FIJAR DÍA 1';
            } else {
                baselineEl.textContent = 'Día 1: ' + store.baseline.day;
                var last = store.samples[store.samples.length - 1];
                daysEl.textContent = last
                    ? edema.daysBetween(store.baseline.day, last.day) + ' días de deriva'
                    : 'La serie empieza en la próxima captura, otro día';
                captureBtn.textContent = 'GUARDAR HOY';
            }

            var points = edema.series(store);
            chartEl.innerHTML = '';
            if (!points.length) {
                chartEl.hidden = true;
            } else {
                chartEl.hidden = false;
                points.slice(-30).forEach(function (point) {
                    var bar = document.createElement('span');
                    bar.className = 'edema-bar ' + edema.band(point.delta);
                    var magnitude = Math.min(22, Math.abs(point.delta) / 8 * 22);
                    bar.style.height = Math.max(2, magnitude) + 'px';
                    if (point.delta < 0) bar.classList.add('down');
                    bar.title = point.day + ' · ' + formatPct(point.delta);
                    chartEl.appendChild(bar);
                });
            }

            historyEl.innerHTML = '';
            points.slice().reverse().slice(0, 6).forEach(function (point) {
                var row = document.createElement('div');
                row.className = 'edema-history-row';
                var day = document.createElement('span');
                day.textContent = point.day + ' · día ' + point.days;
                var value = document.createElement('strong');
                value.textContent = formatPct(point.delta);
                value.className = edema.band(point.delta);
                row.appendChild(day);
                row.appendChild(value);
                historyEl.appendChild(row);
            });

            if (!running) {
                var latest = store.samples[store.samples.length - 1];
                if (latest) paintReadout(latest, 'Última captura guardada vs día 1');
                else paintReadout(null, store.baseline ? 'Línea base fijada' : 'Sin línea base');
            }
        }

        function renderLive() {
            var live = edema.medianSample(buffer);
            captureBtn.disabled = !live;
            if (!live) {
                if (store.baseline) {
                    paintReadout(null, 'Buscando un rostro de frente');
                    deltaEl.textContent = '…';
                }
                return;
            }
            paintReadout(live, store.baseline ? 'Lectura en vivo, todavía no guardada' : 'Lista para fijar el día 1');
        }

        async function ensureLandmarker() {
            if (landmarker) return landmarker;
            setStatus('Cargando Face Landmarker…');
            var vision = await import(VISION_URL);
            var fileset = await vision.FilesetResolver.forVisionTasks(WASM_URL);
            var options = {
                baseOptions: { modelAssetPath: MODEL_URL },
                runningMode: 'VIDEO',
                numFaces: 1
            };
            try {
                options.baseOptions.delegate = 'GPU';
                landmarker = await vision.FaceLandmarker.createFromOptions(fileset, options);
            } catch (gpuError) {
                options.baseOptions.delegate = 'CPU';
                landmarker = await vision.FaceLandmarker.createFromOptions(fileset, options);
            }
            return landmarker;
        }

        function tick() {
            if (!running) return;
            if (video.readyState >= 2 && landmarker) {
                var result = landmarker.detectForVideo(video, performance.now());
                var landmarks = result.faceLandmarks && result.faceLandmarks[0];
                var sample = landmarks ? edema.measure(landmarks) : null;
                if (sample) {
                    buffer.push(sample);
                    if (buffer.length > 24) buffer.shift();
                }
                idle.hidden = true;
                renderLive();
                if (!edema.medianSample(buffer)) {
                    setStatus('Mirá de frente, con el rostro en reposo y bien iluminado.');
                } else if (!store.baseline) {
                    setStatus('Primera captura: queda como día 1. El video no se guarda.');
                } else {
                    setStatus('La lectura en vivo no entra en la serie hasta guardar el día.');
                }
            }
            raf = requestAnimationFrame(tick);
        }

        function stopCamera() {
            running = false;
            cancelAnimationFrame(raf);
            buffer = [];
            if (stream) {
                stream.getTracks().forEach(function (track) { track.stop(); });
                stream = null;
            }
            video.srcObject = null;
            idle.hidden = false;
            cameraBtn.textContent = 'ACTIVAR CÁMARA';
            captureBtn.disabled = true;
            renderStore();
        }

        async function startCamera() {
            if (running || starting) return;
            starting = true;
            try {
                await ensureLandmarker();
                stream = await navigator.mediaDevices.getUserMedia({
                    audio: false,
                    video: { facingMode: 'user', width: { ideal: 640 }, height: { ideal: 480 } }
                });
                video.srcObject = stream;
                await video.play();
                running = true;
                buffer = [];
                cameraBtn.textContent = 'DETENER CÁMARA';
                captureBtn.disabled = true;
                setStatus('Cámara local activa. Ningún frame sale de esta máquina.');
                raf = requestAnimationFrame(tick);
            } catch (err) {
                stopCamera();
                if (err && err.name === 'NotAllowedError') {
                    setStatus('El navegador bloqueó la cámara. Habilitala para este archivo.');
                } else if (err && err.name === 'NotFoundError') {
                    setStatus('No hay una cámara disponible.');
                } else {
                    setStatus('No se pudo abrir la cámara o el modelo. La primera vez hace falta internet para Face Landmarker.');
                }
            } finally {
                starting = false;
            }
        }

        cameraBtn.addEventListener('click', function () {
            if (running) stopCamera();
            else startCamera();
        });

        captureBtn.addEventListener('click', function () {
            var sample = edema.medianSample(buffer);
            if (!sample) return;
            var result = edema.applyCapture(store, sample, new Date());
            if (result.kind === 'same-day') {
                setStatus('El día 1 ya está fijado hoy. La próxima captura, otro día, entra en la serie.');
                return;
            }
            store = result.store;
            edema.saveStore(store);
            renderStore();
            if (result.kind === 'baseline') {
                setStatus('Día 1 guardado. Solo quedaron tres índices en este navegador.');
            } else {
                var last = store.samples[store.samples.length - 1];
                var drift = edema.meanDelta(edema.componentDeltas(last, store.baseline));
                setStatus('Captura del ' + last.day + ' guardada: ' + formatPct(drift) + ' contra el día 1.');
            }
        });

        resetBtn.addEventListener('click', function () {
            if (!store.baseline && !store.samples.length) return;
            if (!window.confirm('¿Borrar el día 1 y toda la serie de hinchazón en esta máquina?')) return;
            store = edema.clearStore();
            renderStore();
            setStatus('Línea base borrada. La próxima captura vuelve a ser el día 1.');
        });

        document.addEventListener('visibilitychange', function () {
            if (document.hidden && running) stopCamera();
        });

        renderStore();
    });
})();
