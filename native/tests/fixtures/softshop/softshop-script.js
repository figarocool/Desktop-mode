/**
 * Script Principale - SoftShop
 * Gestisce il menu hamburger custom e altre funzionalità comuni
 */

document.addEventListener('DOMContentLoaded', function() {
    // ============================================
    // CREA NAVBAR IN JAVASCRIPT
    // ============================================
    const navbarContainer = document.getElementById('navbar-container');
    
    if (navbarContainer) {
        const navbarHTML = `
            <nav class="navbar-custom">
                <div class="navbar-container">
                    <a class="navbar-brand" href="${window.location.origin}">
                        <i class="fas fa-file-invoice-dollar"></i> SoftShop
                    </a>
                    <button class="menu-toggle" id="menuToggle" aria-label="Toggle menu">
                        <span></span>
                        <span></span>
                        <span></span>
                    </button>
                </div>
                
                <div class="menu-overlay" id="menuOverlay"></div>
                
                <div class="menu-sidebar" id="menuSidebar">
                    <ul class="menu-list">
                        <li><a href="/">Home</a></li>
                        <li><a href="/acquista">Acquista</a></li>
                        <li><a href="/single-pc">Single PC</a></li>
                        <li><a href="/multi-pc">Multi PC</a></li>
                        <li><a href="/cloud">Cloud</a></li>
                        <li><a href="/gratuito">Prova Gratis</a></li>
                        <li><a href="/blog">Blog</a></li>
                        <li><a href="/roadmap">Roadmap</a></li>
                        <li class="menu-divider"></li>
                        <li><a href="/login.php"><i class="fas fa-sign-in-alt"></i> Login</a></li>
                        <li><a href="/register.php"><i class="fas fa-user-plus"></i> Registrati</a></li>
                    </ul>
                </div>
            </nav>
        `;
        
        navbarContainer.innerHTML = navbarHTML;
    }
    
    // ============================================
    // CUSTOM HAMBURGER MENU - Navbar custom
    // ============================================
    setTimeout(() => {
        const menuToggle = document.getElementById('menuToggle');
        const menuOverlay = document.getElementById('menuOverlay');
        const menuSidebar = document.getElementById('menuSidebar');
        
        if (menuToggle && menuOverlay && menuSidebar) {
            // Apri/chiudi menu
            menuToggle.addEventListener('click', function() {
                menuToggle.classList.toggle('active');
                menuOverlay.classList.toggle('active');
                menuSidebar.classList.toggle('active');
                document.body.classList.toggle('menu-open');
            });
            
            // Chiudi menu quando si clicca sull'overlay
            menuOverlay.addEventListener('click', function() {
                menuToggle.classList.remove('active');
                menuOverlay.classList.remove('active');
                menuSidebar.classList.remove('active');
                document.body.classList.remove('menu-open');
            });
            
            // Chiudi menu quando si clicca su un link
            const menuLinks = menuSidebar.querySelectorAll('a');
            menuLinks.forEach(link => {
                link.addEventListener('click', function() {
                    menuToggle.classList.remove('active');
                    menuOverlay.classList.remove('active');
                    menuSidebar.classList.remove('active');
                    document.body.classList.remove('menu-open');
                });
            });
        }
    }, 100);
    
    // ============================================
    // HAMBURGER MENU - Header esterno
    // ============================================
    const externalMenuToggle = document.querySelector('.header .menu-toggle');
    const nav = document.querySelector('.header .nav');
    
    if (externalMenuToggle && nav) {
        externalMenuToggle.addEventListener('click', function() {
            externalMenuToggle.classList.toggle('active');
            nav.classList.toggle('active');
            document.body.classList.toggle('menu-open');
        });
        
        // Chiudi menu quando si clicca su un link
        const navLinks = nav.querySelectorAll('a');
        navLinks.forEach(link => {
            link.addEventListener('click', function() {
                externalMenuToggle.classList.remove('active');
                nav.classList.remove('active');
                document.body.classList.remove('menu-open');
            });
        });
        
        // Chiudi menu quando si clicca fuori
        document.addEventListener('click', function(event) {
            const isClickInsideNav = nav.contains(event.target);
            const isClickOnToggle = externalMenuToggle.contains(event.target);
            
            if (!isClickInsideNav && !isClickOnToggle && nav.classList.contains('active')) {
                externalMenuToggle.classList.remove('active');
                nav.classList.remove('active');
                document.body.classList.remove('menu-open');
            }
        });
    }
    
    // ============================================
    // SMOOTH SCROLL
    // ============================================
    document.querySelectorAll('a[href^="#"]').forEach(anchor => {
        anchor.addEventListener('click', function(e) {
            const href = this.getAttribute('href');
            if (href !== '#' && href.length > 1) {
                const target = document.querySelector(href);
                if (target) {
                    e.preventDefault();
                    target.scrollIntoView({
                        behavior: 'smooth',
                        block: 'start'
                    });
                }
            }
        });
    });
    
    // ============================================
    // FORM VALIDATION
    // ============================================
    const forms = document.querySelectorAll('form[data-validate]');
    forms.forEach(form => {
        form.addEventListener('submit', function(e) {
            let isValid = true;
            const inputs = this.querySelectorAll('input[required], textarea[required], select[required]');
            
            inputs.forEach(input => {
                if (!input.value.trim()) {
                    input.classList.add('is-invalid');
                    isValid = false;
                } else {
                    input.classList.remove('is-invalid');
                }
            });
            
            if (!isValid) {
                e.preventDefault();
                alert('Per favore, compila tutti i campi obbligatori.');
            }
        });
    });
    
    // ============================================
    // PAGE VIEW TRACKING (asincrono - non blocca il rendering)
    // ============================================
    // Se l'utente non ha dato consenso pieno (banner cookie), non generiamo/usiamo
    // nessun id persistente: il server (api/track-visit.php) tratterà la richiesta
    // come anonima, senza IP/user-agent e con un id usa-e-getta non collegabile
    // alle altre pagine visitate.
    var hasFullConsent = (typeof window.getAnalyticsConsentParam === 'function') &&
        window.getAnalyticsConsentParam() === 'full';

    var screenRes = screen.width + 'x' + screen.height;
    var trackingFields = {
        page: window.location.pathname,
        consent: hasFullConsent ? 'full' : 'anon',
        screen_resolution: screenRes
    };

    if (hasFullConsent) {
        var trackingId = localStorage.getItem('tracking_session_id');
        if (!trackingId) {
            trackingId = 'xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx'.replace(/[xy]/g, function(c) {
                var r = Math.random() * 16 | 0, v = c === 'x' ? r : (r & 0x3 | 0x8);
                return v.toString(16);
            });
            localStorage.setItem('tracking_session_id', trackingId);
        }
        trackingFields.visitor_sid = trackingId;
    }

    var data = new URLSearchParams(trackingFields);

    if (navigator.sendBeacon) {
        navigator.sendBeacon('/api/track-visit.php', data);
    } else {
        fetch('/api/track-visit.php', { method: 'POST', body: data, keepalive: true });
    }

    // ============================================
    // ALERT AUTO-DISMISS
    // ============================================
    const alerts = document.querySelectorAll('.alert[data-auto-dismiss]');
    alerts.forEach(alert => {
        const delay = parseInt(alert.dataset.autoDismiss) || 5000;
        setTimeout(() => {
            alert.style.opacity = '0';
            setTimeout(() => alert.remove(), 300);
        }, delay);
    });
});
