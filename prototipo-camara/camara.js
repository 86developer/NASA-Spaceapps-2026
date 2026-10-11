// ---------------------------------------------------------------------------
// Detección de fatiga ocular con MediaPipe Face Landmarker
// Prototipo VitalGrid — tarea 12
//
// Qué hace: lee la cámara, calcula la apertura de cada ojo cuadro a cuadro
// y de esa serie saca parpadeos, PERCLOS y microsueños.
// ---------------------------------------------------------------------------

import { FaceLandmarker, FilesetResolver }
  from "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14";

// === CONSTANTES A CONFIRMAR CON EL OFTALMÓLOGO =============================
// Las cuatro están puestas a ojo. Cada una es una pregunta de la entrevista.
const CFG = {
  UMBRAL_CERRADO:     0.12,   // ¿a qué apertura el ojo está funcionalmente cerrado?
  PARPADEO_MAX_MS:    400,    // ¿cuánto dura como máximo un parpadeo normal?
  MICROSUENO_MIN_MS:  500,    // ¿desde qué duración es microsueño?
  VENTANA_PERCLOS_MS: 60000,  // ¿sobre cuántos minutos se promedia PERCLOS?
  FACTOR_CALIBRACION: 0.60,   // umbral = 60 % de la apertura basal propia
  TARGET_FPS:         15,     // Limitador de cuadros (throttling) para no saturar CPU/GPU
  WS_URL:             "ws://127.0.0.1:9002" // Servidor demonio de telemetría VitalGrid
};
// ===========================================================================

// Índices de MediaPipe. Verificalos mirando los puntos dibujados sobre tu cara.
const OJOS = {
  derecho:  { p1: 33,  p2: 160, p3: 158, p4: 133, p5: 153, p6: 144 },
  izquierdo:{ p1: 362, p2: 385, p3: 387, p4: 263, p5: 373, p6: 380 }
};

const $ = id => document.getElementById(id);
const video = $("video"), overlay = $("overlay"), ctx = overlay.getContext("2d");
const curva = $("curva"), cctx = curva.getContext("2d");

let landmarker = null;
let corriendo = false;
let ultimoTs = -1;
let ultimoProcesado = 0;

// --- Conexión WebSocket al Demonio C++ de VitalGrid ---
let ws = null;
let ultimoEnvioTelemetria = 0;

function conectarWebSocket() {
  try {
    ws = new WebSocket(CFG.WS_URL);
    ws.onopen = () => {
      console.log("[PERIPHERAL EYE TRACKER] Conectado a VitalGrid Daemon en", CFG.WS_URL);
      const el = $("v-ws");
      if (el) {
        el.textContent = "ONLINE (CONECTADO)";
        el.style.color = "var(--ok)";
      }
    };
    ws.onclose = () => {
      const el = $("v-ws");
      if (el) {
        el.textContent = "RECONECTANDO...";
        el.style.color = "var(--warn)";
      }
      setTimeout(conectarWebSocket, 2000);
    };
    ws.onerror = () => {
      ws.close();
    };
  } catch (e) {
    setTimeout(conectarWebSocket, 2000);
  }
}

function enviarMetrica(metric, value) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ metric, value }));
  }
}

// --- estado de medición ----------------------------------------------------
let umbral = CFG.UMBRAL_CERRADO;
let estabaCerrado = false;
let tCierre = 0;
let parpadeos = [];     // timestamps, para la frecuencia por minuto
let microsuenos = 0;
let muestras = [];      // { t, cerrado } dentro de la ventana de PERCLOS
let historia = [];      // EAR reciente, solo para dibujar la curva
let calibrando = false, tCal = 0, valoresCal = [];
let fpsT = performance.now(), fpsN = 0;

// --- utilidades ------------------------------------------------------------
const dist = (a, b) => Math.hypot(a.x - b.x, a.y - b.y);

// Apertura del ojo: las dos distancias verticales sobre el doble del ancho.
// Dividir por el ancho hace que el valor no cambie si la persona se acerca.
function ear(pts, ojo) {
  const p = i => pts[ojo[i]];
  const vertical = dist(p("p2"), p("p6")) + dist(p("p3"), p("p5"));
  const ancho    = dist(p("p1"), p("p4"));
  return vertical / (2 * ancho);
}

// --- arranque --------------------------------------------------------------
async function cargarModelo() {
  $("log").textContent = "Cargando modelo…";
  const vision = await FilesetResolver.forVisionTasks(
    "https://cdn.jsdelivr.net/npm/@mediapipe/tasks-vision@0.10.14/wasm"
  );
  landmarker = await FaceLandmarker.createFromOptions(vision, {
    baseOptions: {
      modelAssetPath: "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task",
      delegate: "GPU"
    },
    runningMode: "VIDEO",
    numFaces: 1
  });
  $("log").textContent = "Modelo listo.";
}

async function encender() {
  $("btn-start").disabled = true;
  try {
    if (!landmarker) await cargarModelo();
    const stream = await navigator.mediaDevices.getUserMedia({
      video: { width: 640, height: 480 }
    });
    video.srcObject = stream;
    await video.play();
    overlay.width = video.videoWidth;
    overlay.height = video.videoHeight;
    corriendo = true;
    $("btn-cal").disabled = false;
    $("btn-reset").disabled = false;
    $("log").textContent = "Midiendo. El video no se guarda en ningún lado.";
    requestAnimationFrame(bucle);
  } catch (e) {
    $("log").textContent = "Error: " + e.message +
      " — recordá abrir la página con Live Server, no con doble clic.";
    $("btn-start").disabled = false;
  }
}

// --- bucle principal -------------------------------------------------------
function bucle() {
  if (!corriendo) return;
  const ahora = performance.now();
  const intervaloMs = 1000 / CFG.TARGET_FPS;

  // Throttling: ejecutar inferencia a máximo 15 FPS para reducir consumo térmico/CPU
  if ((ahora - ultimoProcesado >= intervaloMs) && video.currentTime !== ultimoTs) {
    ultimoProcesado = ahora;
    ultimoTs = video.currentTime;
    const res = landmarker.detectForVideo(video, ahora);

    if (res.faceLandmarks && res.faceLandmarks.length) {
      const pts = res.faceLandmarks[0];
      const e = (ear(pts, OJOS.derecho) + ear(pts, OJOS.izquierdo)) / 2;
      procesar(e, ahora);
      dibujarPuntos(pts);
    } else {
      ctx.clearRect(0, 0, overlay.width, overlay.height);
      $("log").textContent = "No veo una cara. Acercate o mejorá la luz.";
    }
  }

  fpsN++;
  if (ahora - fpsT > 1000) {
    $("v-fps").textContent = fpsN;
    fpsN = 0; fpsT = ahora;
  }

  dibujarCurva();
  requestAnimationFrame(bucle);
}

// --- lógica de medición ----------------------------------------------------
function procesar(e, t) {
  // calibración: cinco segundos con los ojos abiertos fijan el umbral propio
  if (calibrando) {
    valoresCal.push(e);
    const restan = Math.ceil((5000 - (t - tCal)) / 1000);
    $("log").textContent = `Calibrando… mantené los ojos abiertos (${restan})`;
    if (t - tCal >= 5000) {
      valoresCal.sort((a, b) => a - b);
      const basal = valoresCal[Math.floor(valoresCal.length / 2)]; // mediana
      umbral = basal * CFG.FACTOR_CALIBRACION;
      calibrando = false;
      $("v-thr").textContent = umbral.toFixed(3);
      $("log").textContent = `Listo. Apertura basal ${basal.toFixed(3)} · umbral ${umbral.toFixed(3)}`;
    }
  }

  const cerrado = e < umbral;

  // máquina de estados: al abrir el ojo clasificamos el cierre que terminó
  if (cerrado && !estabaCerrado) {
    tCierre = t;
  } else if (!cerrado && estabaCerrado) {
    const dur = t - tCierre;
    $("v-dur").textContent = Math.round(dur) + " ms";
    if (dur <= CFG.PARPADEO_MAX_MS) {
      parpadeos.push(t);                       // V angosta
    } else if (dur >= CFG.MICROSUENO_MIN_MS) {
      microsuenos++;                           // U ancha
      $("v-micro").textContent = microsuenos;
      // Ingestión de telemetría de evento crítico: microsueño detectado
      enviarMetrica("microsleep", microsuenos);
    }
    // entre ambos umbrales queda sin clasificar: preguntarle al oftalmólogo
  }
  estabaCerrado = cerrado;

  // PERCLOS: proporción de tiempo con el ojo cerrado dentro de la ventana
  muestras.push({ t, cerrado });
  const desde = t - CFG.VENTANA_PERCLOS_MS;
  while (muestras.length && muestras[0].t < desde) muestras.shift();
  const perclos = muestras.length
    ? muestras.filter(m => m.cerrado).length / muestras.length : 0;

  // frecuencia de parpadeo del último minuto
  while (parpadeos.length && parpadeos[0] < t - 60000) parpadeos.shift();

  historia.push(e);
  if (historia.length > 300) historia.shift();

  // Enviar telemetría periférica periódica a 1 Hz hacia VitalGrid Daemon (ws://127.0.0.1:9002)
  if (t - ultimoEnvioTelemetria >= 1000) {
    ultimoEnvioTelemetria = t;
    enviarMetrica("blink_rate", parpadeos.length);
    enviarMetrica("microsleep", microsuenos);
    enviarMetrica("perclos", Number((perclos * 100).toFixed(1)));
  }

  // pantalla
  $("v-ear").textContent = e.toFixed(3);
  $("v-perclos").textContent = (perclos * 100).toFixed(1) + " %";
  $("v-rate").textContent = parpadeos.length;
  $("v-blinks").textContent = parpadeos.length;
  const est = $("estado");
  est.className = "estado " + (cerrado ? "cerrado" : "abierto");
  est.textContent = cerrado ? "Ojo cerrado" : "Ojo abierto";
}

// --- dibujo ----------------------------------------------------------------
function dibujarPuntos(pts) {
  const W = overlay.width, H = overlay.height;
  ctx.clearRect(0, 0, W, H);
  for (const ojo of Object.values(OJOS)) {
    for (const k of ["p1", "p2", "p3", "p4", "p5", "p6"]) {
      const p = pts[ojo[k]];
      ctx.beginPath();
      ctx.arc(p.x * W, p.y * H, 3, 0, Math.PI * 2);
      ctx.fillStyle = "#5fa8d3";
      ctx.fill();
    }
  }
}

let anchoCurvaPrevio = 0;
function dibujarCurva() {
  const W = curva.clientWidth;
  if (W !== anchoCurvaPrevio) {
    curva.width = W;
    curva.height = 150;
    anchoCurvaPrevio = W;
  }
  const H = 150;
  cctx.clearRect(0, 0, W, H);
  const MAX = 0.45;
  const y = v => H - (v / MAX) * H;

  // línea del umbral
  cctx.strokeStyle = "#d4574c";
  cctx.setLineDash([6, 5]);
  cctx.beginPath(); cctx.moveTo(0, y(umbral)); cctx.lineTo(W, y(umbral)); cctx.stroke();
  cctx.setLineDash([]);

  if (historia.length < 2) return;
  cctx.strokeStyle = "#5fa8d3";
  cctx.lineWidth = 2;
  cctx.lineJoin = "round";
  cctx.beginPath();
  historia.forEach((v, i) => {
    const x = (i / (historia.length - 1)) * W;
    i ? cctx.lineTo(x, y(v)) : cctx.moveTo(x, y(v));
  });
  cctx.stroke();
}

// --- controles -------------------------------------------------------------
$("btn-start").addEventListener("click", encender);

$("btn-cal").addEventListener("click", () => {
  calibrando = true; tCal = performance.now(); valoresCal = [];
});

$("btn-reset").addEventListener("click", () => {
  parpadeos = []; microsuenos = 0; muestras = []; historia = [];
  $("v-micro").textContent = "0";
  $("v-dur").textContent = "—";
  $("log").textContent = "Conteo reiniciado.";
  enviarMetrica("blink_rate", 0);
  enviarMetrica("microsleep", 0);
  enviarMetrica("perclos", 0);
});

// Inicializar enlace de comunicación con el daemon en background
conectarWebSocket();
