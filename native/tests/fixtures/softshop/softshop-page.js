/**
 * Page Time Tracking
 * Misura il tempo "attivo" che l'utente passa su questa pagina (in pausa
 * quando la scheda non e' in primo piano, cosi' non si gonfia il dato se il
 * tab resta aperto in background) e lo scroll massimo raggiunto. Invia il
 * dato una sola volta, quando l'utente lascia la pagina.
 */
(function () {
    'use strict';

    var TRACKING_URL = '/api/track-page-time.php';
    var MIN_SECONDS_TO_SEND = 3; // sotto questa soglia e' un rimbalzo, non lo contiamo

    var activeMs = 0;
    var lastResumeAt = document.hidden ? null : Date.now();
    var maxScrollPct = 0;
    var sent = false;

    function updateScroll() {
        var doc = document.documentElement;
        var scrollable = (doc.scrollHeight - doc.clientHeight);
        if (scrollable <= 0) {
            maxScrollPct = 100;
            return;
        }
        var pct = Math.round((window.scrollY / scrollable) * 100);
        if (pct > maxScrollPct) maxScrollPct = Math.min(100, pct);
    }

    function onVisibilityChange() {
        if (document.hidden) {
            if (lastResumeAt !== null) {
                activeMs += Date.now() - lastResumeAt;
                lastResumeAt = null;
            }
        } else {
            lastResumeAt = Date.now();
        }
    }

    function getConsentParam() {
        try {
            return (typeof window.getAnalyticsConsentParam === 'function')
                ? window.getAnalyticsConsentParam()
                : 'anon';
        } catch (e) {
            return 'anon';
        }
    }

    function send() {
        if (sent) return;
        if (lastResumeAt !== null) {
            activeMs += Date.now() - lastResumeAt;
            lastResumeAt = null;
        }
        var seconds = Math.round(activeMs / 1000);
        if (seconds < MIN_SECONDS_TO_SEND) return;
        sent = true;

        var payload = new URLSearchParams();
        payload.set('pagina', window.location.pathname);
        payload.set('tempo_secondi', String(seconds));
        payload.set('scroll_max', String(maxScrollPct));
        payload.set('consent', getConsentParam());

        try {
            if (navigator.sendBeacon) {
                var blob = new Blob([payload.toString()], { type: 'application/x-www-form-urlencoded' });
                navigator.sendBeacon(TRACKING_URL, blob);
            } else {
                var xhr = new XMLHttpRequest();
                xhr.open('POST', TRACKING_URL, false);
                xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');
                xhr.send(payload.toString());
            }
        } catch (e) {
            // Silenziosamente ignora: non deve mai interrompere la navigazione.
        }
    }

    window.addEventListener('scroll', updateScroll, { passive: true });
    document.addEventListener('visibilitychange', onVisibilityChange);
    window.addEventListener('pagehide', send);
    window.addEventListener('beforeunload', send);
})();
