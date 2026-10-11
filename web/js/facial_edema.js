/**
 * VitalGrid — índice de hinchazón facial contra el día 1.
 *
 * Los anchos de pómulos y mandíbula, y la relación ancho/alto, se dividen
 * por la distancia intercantal (esquinas internas de los ojos). Esa referencia
 * casi no cambia con el edema blando ni con la distancia a la cámara, así que
 * un rostro más cerca no parece más hinchado.
 *
 * No guarda imágenes. La serie es una captura por día: la deriva que importa
 * es la de semanas y meses, no el ruido de un frame.
 */
(function (root) {
    'use strict';

    var IDX = {
        nose: 1,
        forehead: 10,
        chin: 152,
        leftInner: 133,
        rightInner: 362,
        leftCheek: 234,
        rightCheek: 454,
        leftJaw: 172,
        rightJaw: 397
    };

    var STORAGE_KEY = 'vitalgrid.facialEdema.v1';
    var MIN_INTERCANTHAL = 0.04;
    var MAX_ASYMMETRY = 0.22;
    var MAX_SAMPLES = 400;

    function dist(a, b) {
        var dx = a.x - b.x;
        var dy = a.y - b.y;
        return Math.hypot(dx, dy);
    }

    function finitePoint(point) {
        return point && Number.isFinite(point.x) && Number.isFinite(point.y);
    }

    function measure(landmarks) {
        if (!landmarks || landmarks.length < 468) return null;
        var leftInner = landmarks[IDX.leftInner];
        var rightInner = landmarks[IDX.rightInner];
        var leftCheek = landmarks[IDX.leftCheek];
        var rightCheek = landmarks[IDX.rightCheek];
        var leftJaw = landmarks[IDX.leftJaw];
        var rightJaw = landmarks[IDX.rightJaw];
        var forehead = landmarks[IDX.forehead];
        var chin = landmarks[IDX.chin];
        var nose = landmarks[IDX.nose];
        var points = [leftInner, rightInner, leftCheek, rightCheek, leftJaw, rightJaw, forehead, chin, nose];
        for (var i = 0; i < points.length; i++) {
            if (!finitePoint(points[i])) return null;
        }

        var inter = dist(leftInner, rightInner);
        if (inter < MIN_INTERCANTHAL) return null;

        var minX = Math.min(leftInner.x, rightInner.x);
        var maxX = Math.max(leftInner.x, rightInner.x);
        if (nose.x < minX - inter * 0.35 || nose.x > maxX + inter * 0.35) return null;

        var leftSpan = Math.abs(leftCheek.x - nose.x);
        var rightSpan = Math.abs(rightCheek.x - nose.x);
        var asymmetry = Math.abs(leftSpan - rightSpan) / Math.max(leftSpan, rightSpan, 1e-6);
        if (asymmetry > MAX_ASYMMETRY) return null;

        var cheekWidth = dist(leftCheek, rightCheek);
        var jawWidth = dist(leftJaw, rightJaw);
        var faceHeight = dist(forehead, chin);
        if (faceHeight < 1e-6) return null;

        return {
            cheek: cheekWidth / inter,
            jaw: jawWidth / inter,
            roundness: cheekWidth / faceHeight
        };
    }

    function percent(current, baseline) {
        if (!Number.isFinite(current) || !Number.isFinite(baseline) || baseline === 0) return null;
        return ((current - baseline) / baseline) * 100;
    }

    function componentDeltas(current, baseline) {
        if (!current || !baseline) return null;
        var cheek = percent(current.cheek, baseline.cheek);
        var jaw = percent(current.jaw, baseline.jaw);
        var roundness = percent(current.roundness, baseline.roundness);
        if (cheek === null || jaw === null || roundness === null) return null;
        return { cheek: cheek, jaw: jaw, roundness: roundness };
    }

    function meanDelta(deltas) {
        if (!deltas) return null;
        return (deltas.cheek + deltas.jaw + deltas.roundness) / 3;
    }

    function band(delta) {
        var abs = Math.abs(delta);
        if (abs < 2) return 'nominal';
        if (abs < 6) return 'warn';
        return 'crit';
    }

    function median(values) {
        if (!values.length) return null;
        var sorted = values.slice().sort(function (a, b) { return a - b; });
        var mid = Math.floor(sorted.length / 2);
        if (sorted.length % 2) return sorted[mid];
        return (sorted[mid - 1] + sorted[mid]) / 2;
    }

    function medianSample(frames) {
        if (!frames || frames.length < 8) return null;
        var sample = {
            cheek: median(frames.map(function (frame) { return frame.cheek; })),
            jaw: median(frames.map(function (frame) { return frame.jaw; })),
            roundness: median(frames.map(function (frame) { return frame.roundness; }))
        };
        if (!Number.isFinite(sample.cheek) || !Number.isFinite(sample.jaw) || !Number.isFinite(sample.roundness)) {
            return null;
        }
        return sample;
    }

    function dayKey(date) {
        var y = date.getFullYear();
        var m = String(date.getMonth() + 1).padStart(2, '0');
        var d = String(date.getDate()).padStart(2, '0');
        return y + '-' + m + '-' + d;
    }

    function daysBetween(startDay, endDay) {
        var start = new Date(startDay + 'T00:00:00');
        var end = new Date(endDay + 'T00:00:00');
        return Math.round((end.getTime() - start.getTime()) / 86400000);
    }

    function emptyStore() {
        return { baseline: null, samples: [] };
    }

    function validSample(sample) {
        return sample && Number.isFinite(sample.cheek) && Number.isFinite(sample.jaw) && Number.isFinite(sample.roundness);
    }

    function loadStore() {
        try {
            var raw = root.localStorage.getItem(STORAGE_KEY);
            if (!raw) return emptyStore();
            var data = JSON.parse(raw);
            var samples = Array.isArray(data.samples) ? data.samples.filter(validSample) : [];
            var baseline = validSample(data.baseline) ? data.baseline : null;
            return { baseline: baseline, samples: samples };
        } catch (err) {
            return emptyStore();
        }
    }

    function saveStore(store) {
        root.localStorage.setItem(STORAGE_KEY, JSON.stringify({
            baseline: store.baseline,
            samples: store.samples
        }));
    }

    function clearStore() {
        root.localStorage.removeItem(STORAGE_KEY);
        return emptyStore();
    }

    function applyCapture(store, sample, now) {
        var day = dayKey(now);
        var stamped = {
            cheek: sample.cheek,
            jaw: sample.jaw,
            roundness: sample.roundness,
            savedAt: now.toISOString(),
            day: day
        };
        if (!store.baseline) {
            return {
                store: { baseline: stamped, samples: [] },
                kind: 'baseline'
            };
        }
        if (store.baseline.day === day) {
            return { store: store, kind: 'same-day' };
        }
        var samples = store.samples.filter(function (item) { return item.day !== day; });
        samples.push(stamped);
        samples.sort(function (a, b) { return a.day < b.day ? -1 : a.day > b.day ? 1 : 0; });
        if (samples.length > MAX_SAMPLES) samples = samples.slice(samples.length - MAX_SAMPLES);
        return {
            store: { baseline: store.baseline, samples: samples },
            kind: 'saved'
        };
    }

    function series(store) {
        if (!store.baseline) return [];
        return store.samples.map(function (sample) {
            var deltas = componentDeltas(sample, store.baseline);
            return {
                day: sample.day,
                delta: meanDelta(deltas),
                days: daysBetween(store.baseline.day, sample.day)
            };
        });
    }

    function selfTest() {
        var landmarks = new Array(478);
        for (var i = 0; i < landmarks.length; i++) landmarks[i] = { x: 0, y: 0, z: 0 };
        landmarks[133] = { x: 0.40, y: 0.40, z: 0 };
        landmarks[362] = { x: 0.60, y: 0.40, z: 0 };
        landmarks[234] = { x: 0.20, y: 0.50, z: 0 };
        landmarks[454] = { x: 0.80, y: 0.50, z: 0 };
        landmarks[172] = { x: 0.30, y: 0.70, z: 0 };
        landmarks[397] = { x: 0.70, y: 0.70, z: 0 };
        landmarks[10] = { x: 0.50, y: 0.15, z: 0 };
        landmarks[152] = { x: 0.50, y: 0.90, z: 0 };
        landmarks[1] = { x: 0.50, y: 0.45, z: 0 };

        var sample = measure(landmarks);
        if (!sample) throw new Error('la cara de prueba fue rechazada');
        if (Math.abs(sample.cheek - 3) > 1e-9) throw new Error('pómulo esperado 3');
        if (Math.abs(sample.jaw - 2) > 1e-9) throw new Error('mandíbula esperada 2');
        if (Math.abs(sample.roundness - 0.8) > 1e-9) throw new Error('ancho/alto esperado 0.8');

        landmarks[234] = { x: 0.10, y: 0.50, z: 0 };
        if (measure(landmarks) !== null) throw new Error('un rostro girado no debe medirse');

        var deltas = componentDeltas(
            { cheek: 3.15, jaw: 2.10, roundness: 0.84 },
            { cheek: 3, jaw: 2, roundness: 0.8 }
        );
        var drift = meanDelta(deltas);
        if (Math.abs(drift - 5) > 1e-9) throw new Error('la deriva del 5% no cierra');
        if (band(drift) !== 'warn') throw new Error('5% tiene que marcar deriva');
        if (band(1.2) !== 'nominal' || band(7) !== 'crit') throw new Error('bandas incorrectas');

        var frames = [];
        for (var n = 0; n < 8; n++) frames.push({ cheek: n + 1, jaw: 2, roundness: 0.8 });
        var mid = medianSample(frames);
        if (!mid || Math.abs(mid.cheek - 4.5) > 1e-9) throw new Error('mediana incorrecta');
        if (medianSample(frames.slice(0, 7)) !== null) throw new Error('hacen falta 8 frames');

        var stored = applyCapture(emptyStore(), sample, new Date(2026, 0, 1, 12));
        if (stored.kind !== 'baseline' || stored.store.samples.length !== 0) {
            throw new Error('la primera captura es el día 1');
        }
        var sameDay = applyCapture(stored.store, sample, new Date(2026, 0, 1, 18));
        if (sameDay.kind !== 'same-day') throw new Error('el mismo día no entra en la serie');
        var later = applyCapture(stored.store, { cheek: 3.15, jaw: 2.1, roundness: 0.84 }, new Date(2026, 3, 1, 9));
        if (later.kind !== 'saved' || later.store.samples.length !== 1) throw new Error('otro día se guarda');
        var points = series(later.store);
        if (points.length !== 1 || points[0].days !== 90 || Math.abs(points[0].delta - 5) > 1e-9) {
            throw new Error('la serie no mide los 90 días');
        }
        var replaced = applyCapture(later.store, { cheek: 3.3, jaw: 2.2, roundness: 0.88 }, new Date(2026, 3, 1, 20));
        if (replaced.store.samples.length !== 1 || replaced.store.samples[0].cheek !== 3.3) {
            throw new Error('una segunda captura del mismo día reemplaza la anterior');
        }
    }

    root.FacialEdema = {
        STORAGE_KEY: STORAGE_KEY,
        measure: measure,
        componentDeltas: componentDeltas,
        meanDelta: meanDelta,
        band: band,
        medianSample: medianSample,
        dayKey: dayKey,
        daysBetween: daysBetween,
        emptyStore: emptyStore,
        loadStore: loadStore,
        saveStore: saveStore,
        clearStore: clearStore,
        applyCapture: applyCapture,
        series: series,
        selfTest: selfTest
    };
})(typeof window !== 'undefined' ? window : globalThis);
