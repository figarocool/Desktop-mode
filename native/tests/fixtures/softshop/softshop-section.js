/**
 * Section Tracking
 * Traccia quali sezioni del sito gli utenti leggono
 */

(function() {
    'use strict';

    // Configurazione
    const config = {
        trackingUrl: '/api/track-section.php',
        minTimeOnSection: 2000, // 2 secondi minimo prima di tracciare
        scrollThreshold: 0.3 // 30% della sezione visibile
    };

    // Stato del tracking
    const state = {
        sections: {},
        currentSections: new Set(),
        sessionStart: Date.now()
    };

    /**
     * Inizializza il tracking delle sezioni
     */
    function initSectionTracking() {
        // Trova tutte le sezioni tracciabili
        const sections = document.querySelectorAll('section[data-track-section]');
        
        sections.forEach(section => {
            const sectionId = section.getAttribute('data-track-section');
            state.sections[sectionId] = {
                id: sectionId,
                name: section.getAttribute('data-section-name') || sectionId,
                startTime: null,
                totalTime: 0,
                views: 0,
                scrollDepth: 0
            };
        });

        // Usa Intersection Observer per tracciare visibilità
        if ('IntersectionObserver' in window) {
            const observer = new IntersectionObserver(handleIntersection, {
                threshold: [0, 0.25, 0.5, 0.75, 1]
            });

            document.querySelectorAll('section[data-track-section]').forEach(section => {
                observer.observe(section);
            });
        }

        // Traccia scroll
        window.addEventListener('scroll', handleScroll, { passive: true });
        
        // Traccia quando l'utente lascia la pagina
        window.addEventListener('beforeunload', handlePageUnload);
        
        // Traccia quando la pagina perde focus
        document.addEventListener('visibilitychange', handleVisibilityChange);
    }

    /**
     * Gestisce l'intersezione delle sezioni
     */
    function handleIntersection(entries) {
        entries.forEach(entry => {
            const sectionId = entry.target.getAttribute('data-track-section');
            
            if (entry.isIntersecting) {
                // Sezione diventa visibile
                if (!state.currentSections.has(sectionId)) {
                    state.currentSections.add(sectionId);
                    state.sections[sectionId].startTime = Date.now();
                    state.sections[sectionId].views++;
                }
            } else {
                // Sezione non è più visibile
                if (state.currentSections.has(sectionId)) {
                    state.currentSections.delete(sectionId);
                    
                    // Calcola il tempo trascorso
                    if (state.sections[sectionId].startTime) {
                        const timeOnSection = Date.now() - state.sections[sectionId].startTime;
                        state.sections[sectionId].totalTime += timeOnSection;
                        state.sections[sectionId].startTime = null;
                    }
                }
            }
        });
    }

    /**
     * Gestisce lo scroll della pagina
     */
    function handleScroll() {
        const scrollPercentage = (window.scrollY / (document.documentElement.scrollHeight - window.innerHeight)) * 100;
        
        document.querySelectorAll('section[data-track-section]').forEach(section => {
            const sectionId = section.getAttribute('data-track-section');
            const rect = section.getBoundingClientRect();
            
            // Calcola la profondità di scroll della sezione
            if (rect.top < window.innerHeight && rect.bottom > 0) {
                const visiblePercentage = Math.min(100, Math.max(0, 
                    (window.innerHeight - rect.top) / (window.innerHeight + rect.height) * 100
                ));
                
                if (visiblePercentage > state.sections[sectionId].scrollDepth) {
                    state.sections[sectionId].scrollDepth = visiblePercentage;
                }
            }
        });
    }

    /**
     * Gestisce quando la pagina perde focus
     */
    function handleVisibilityChange() {
        if (document.hidden) {
            // Pagina nascosta - salva il tempo corrente
            state.currentSections.forEach(sectionId => {
                if (state.sections[sectionId].startTime) {
                    const timeOnSection = Date.now() - state.sections[sectionId].startTime;
                    state.sections[sectionId].totalTime += timeOnSection;
                    state.sections[sectionId].startTime = null;
                }
            });
        } else {
            // Pagina visibile di nuovo - ricomincia il tracking
            state.currentSections.forEach(sectionId => {
                state.sections[sectionId].startTime = Date.now();
            });
        }
    }

    /**
     * Gestisce quando l'utente lascia la pagina
     */
    function handlePageUnload() {
        // Finalizza il tempo per tutte le sezioni correnti
        state.currentSections.forEach(sectionId => {
            if (state.sections[sectionId].startTime) {
                const timeOnSection = Date.now() - state.sections[sectionId].startTime;
                state.sections[sectionId].totalTime += timeOnSection;
            }
        });

        // Invia i dati di tracking
        sendTrackingData();
    }

    /**
     * Invia i dati di tracking al server
     */
    function sendTrackingData() {
        const trackingData = [];

        Object.values(state.sections).forEach(section => {
            // Traccia solo sezioni con tempo minimo
            if (section.totalTime >= config.minTimeOnSection || section.scrollDepth > 50) {
                trackingData.push({
                    section_id: section.id,
                    section_name: section.name,
                    time_on_section: Math.round(section.totalTime / 1000), // Converti in secondi
                    scroll_depth: Math.round(section.scrollDepth),
                    views: section.views,
                    page_url: window.location.pathname
                });
            }
        });

        if (trackingData.length > 0) {
            // Usa sendBeacon per garantire l'invio anche se la pagina si chiude
            const hasFullConsent = (typeof window.getAnalyticsConsentParam === 'function') &&
                window.getAnalyticsConsentParam() === 'full';
            const data = new FormData();
            data.append('tracking_data', JSON.stringify(trackingData));
            data.append('consent', hasFullConsent ? 'full' : 'anon');

            if (navigator.sendBeacon) {
                navigator.sendBeacon(config.trackingUrl, data);
            } else {
                // Fallback per browser vecchi
                fetch(config.trackingUrl, {
                    method: 'POST',
                    body: data,
                    keepalive: true
                }).catch(() => {
                    // Ignora errori
                });
            }
        }
    }

    /**
     * Invia periodicamente i dati di tracking (ogni 30 secondi)
     */
    function startPeriodicTracking() {
        setInterval(() => {
            sendTrackingData();
        }, 30000);
    }

    // Inizializza quando il DOM è pronto
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', () => {
            initSectionTracking();
            startPeriodicTracking();
        });
    } else {
        initSectionTracking();
        startPeriodicTracking();
    }
})();


/**
 * Screen Resolution Tracking
 * Traccia la risoluzione dello schermo dell'utente
 */
(function() {
    'use strict';

    /**
     * Invia la risoluzione dello schermo al server
     */
    function trackScreenResolution() {
        // Ottieni la risoluzione dello schermo
        const resolution = screen.width + 'x' + screen.height;
        
        // Invia al server
        fetch('/api/track-screen-resolution.php', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({
                resolution: resolution
            })
        }).catch(() => {
            // Ignora errori silenziosamente
        });
    }

    // Traccia la risoluzione quando la pagina è caricata
    if (document.readyState === 'loading') {
        document.addEventListener('DOMContentLoaded', trackScreenResolution);
    } else {
        trackScreenResolution();
    }
})();
