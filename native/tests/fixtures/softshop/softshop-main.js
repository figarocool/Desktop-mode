/**
 * Script Principale
 * Funzioni JavaScript comuni per l'applicazione
 */

// ============================================
// UTILITY FUNCTIONS
// ============================================

/**
 * Mostra un messaggio di notifica
 */
function showNotification(message, type = 'success') {
    const alertClass = type === 'error' ? 'danger' : type;
    const icon = type === 'error' ? 'exclamation-circle' : 'check-circle';
    
    const alertHTML = `
        <div class="alert alert-${alertClass} alert-dismissible fade show" role="alert">
            <i class="fas fa-${icon}"></i> ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        </div>
    `;
    
    // Inserisci l'alert all'inizio del main
    const main = document.querySelector('main');
    if (main) {
        main.insertAdjacentHTML('afterbegin', alertHTML);
    }
}

/**
 * Formatta un importo in valuta
 */
function formatCurrency(amount) {
    return new Intl.NumberFormat('it-IT', {
        style: 'currency',
        currency: 'EUR'
    }).format(amount);
}

/**
 * Formatta una data
 */
function formatDate(dateString) {
    const date = new Date(dateString);
    return new Intl.DateTimeFormat('it-IT', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit'
    }).format(date);
}

/**
 * Formatta una data e ora
 */
function formatDateTime(dateString) {
    const date = new Date(dateString);
    return new Intl.DateTimeFormat('it-IT', {
        year: 'numeric',
        month: '2-digit',
        day: '2-digit',
        hour: '2-digit',
        minute: '2-digit'
    }).format(date);
}

/**
 * Verifica se un email è valido
 */
function isValidEmail(email) {
    const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
    return emailRegex.test(email);
}

/**
 * Verifica se una password è valida
 */
function isValidPassword(password) {
    // Minimo 8 caratteri, almeno una maiuscola, una minuscola, un numero
    const passwordRegex = /^(?=.*[a-z])(?=.*[A-Z])(?=.*\d).{8,}$/;
    return passwordRegex.test(password);
}

// ============================================
// FORM VALIDATION
// ============================================

/**
 * Valida un form
 */
function validateForm(formId) {
    const form = document.getElementById(formId);
    if (!form) return false;
    
    let isValid = true;
    const inputs = form.querySelectorAll('input, textarea, select');
    
    inputs.forEach(input => {
        if (input.hasAttribute('required') && !input.value.trim()) {
            input.classList.add('is-invalid');
            isValid = false;
        } else {
            input.classList.remove('is-invalid');
        }
    });
    
    return isValid;
}

/**
 * Aggiungi validazione in tempo reale ai form
 */
document.addEventListener('DOMContentLoaded', function() {
    const forms = document.querySelectorAll('form');
    
    forms.forEach(form => {
        const inputs = form.querySelectorAll('input, textarea, select');
        
        inputs.forEach(input => {
            input.addEventListener('blur', function() {
                if (this.hasAttribute('required') && !this.value.trim()) {
                    this.classList.add('is-invalid');
                } else {
                    this.classList.remove('is-invalid');
                }
            });
        });
    });
});

// ============================================
// AJAX REQUESTS
// ============================================

/**
 * Effettua una richiesta AJAX
 */
function makeAjaxRequest(url, method = 'GET', data = null) {
    return new Promise((resolve, reject) => {
        const options = {
            method: method,
            headers: {
                'X-Requested-With': 'XMLHttpRequest'
            }
        };
        
        if (data && method !== 'GET') {
            options.body = new FormData(data);
        }
        
        fetch(url, options)
            .then(response => response.json())
            .then(data => resolve(data))
            .catch(error => reject(error));
    });
}

// ============================================
// TABLE UTILITIES
// ============================================

/**
 * Aggiungi funzionalità di ricerca a una tabella
 */
function addTableSearch(tableId, searchInputId) {
    const searchInput = document.getElementById(searchInputId);
    const table = document.getElementById(tableId);
    
    if (!searchInput || !table) return;
    
    searchInput.addEventListener('keyup', function() {
        const searchTerm = this.value.toLowerCase();
        const rows = table.querySelectorAll('tbody tr');
        
        rows.forEach(row => {
            const text = row.textContent.toLowerCase();
            row.style.display = text.includes(searchTerm) ? '' : 'none';
        });
    });
}

/**
 * Aggiungi funzionalità di ordinamento a una tabella
 */
function addTableSort(tableId) {
    const table = document.getElementById(tableId);
    if (!table) return;
    
    const headers = table.querySelectorAll('thead th');
    
    headers.forEach((header, index) => {
        header.style.cursor = 'pointer';
        header.addEventListener('click', function() {
            sortTable(table, index);
        });
    });
}

/**
 * Ordina una tabella
 */
function sortTable(table, columnIndex) {
    const rows = Array.from(table.querySelectorAll('tbody tr'));
    const isAscending = table.dataset.sortOrder !== 'asc';
    
    rows.sort((a, b) => {
        const aValue = a.cells[columnIndex].textContent.trim();
        const bValue = b.cells[columnIndex].textContent.trim();
        
        // Prova a convertire in numero
        const aNum = parseFloat(aValue);
        const bNum = parseFloat(bValue);
        
        if (!isNaN(aNum) && !isNaN(bNum)) {
            return isAscending ? aNum - bNum : bNum - aNum;
        }
        
        return isAscending ? 
            aValue.localeCompare(bValue) : 
            bValue.localeCompare(aValue);
    });
    
    rows.forEach(row => table.querySelector('tbody').appendChild(row));
    table.dataset.sortOrder = isAscending ? 'asc' : 'desc';
}

// ============================================
// MODAL UTILITIES
// ============================================

/**
 * Mostra un modal di conferma
 */
function showConfirmModal(title, message, onConfirm) {
    const modalHTML = `
        <div class="modal fade" id="confirmModal" tabindex="-1">
            <div class="modal-dialog">
                <div class="modal-content">
                    <div class="modal-header">
                        <h5 class="modal-title">${title}</h5>
                        <button type="button" class="btn-close" data-bs-dismiss="modal"></button>
                    </div>
                    <div class="modal-body">
                        ${message}
                    </div>
                    <div class="modal-footer">
                        <button type="button" class="btn btn-secondary" data-bs-dismiss="modal">
                            Annulla
                        </button>
                        <button type="button" class="btn btn-primary" id="confirmBtn">
                            Conferma
                        </button>
                    </div>
                </div>
            </div>
        </div>
    `;
    
    document.body.insertAdjacentHTML('beforeend', modalHTML);
    const modal = new bootstrap.Modal(document.getElementById('confirmModal'));
    
    document.getElementById('confirmBtn').addEventListener('click', function() {
        onConfirm();
        modal.hide();
        document.getElementById('confirmModal').remove();
    });
    
    modal.show();
}

// ============================================
// INITIALIZATION
// ============================================

document.addEventListener('DOMContentLoaded', function() {
    // Inizializza i tooltip di Bootstrap
    const tooltipTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="tooltip"]'));
    tooltipTriggerList.map(function(tooltipTriggerEl) {
        return new bootstrap.Tooltip(tooltipTriggerEl);
    });
    
    // Inizializza i popover di Bootstrap
    const popoverTriggerList = [].slice.call(document.querySelectorAll('[data-bs-toggle="popover"]'));
    popoverTriggerList.map(function(popoverTriggerEl) {
        return new bootstrap.Popover(popoverTriggerEl);
    });
});


// ============================================
// GOOGLE PING SCHEDULER - AJAX
// ============================================

/**
 * Esegui il Google Ping Scheduler in background dopo il caricamento della pagina
 * Non blocca il caricamento della pagina
 */
document.addEventListener('DOMContentLoaded', function() {
    // Usa setTimeout per eseguire il ping dopo che la pagina è completamente caricata
    setTimeout(function() {
        runGooglePingScheduler();
    }, 1000); // Aspetta 1 secondo dopo il caricamento
});

/**
 * Chiama l'API per eseguire il Google Ping Scheduler
 */
function runGooglePingScheduler() {
    // Usa fetch con keepalive per eseguire la richiesta anche se l'utente naviga via
    fetch('/api/run-google-ping.php', {
        method: 'GET',
        keepalive: true,
        signal: AbortSignal.timeout(5000) // Timeout di 5 secondi
    })
    .catch(error => {
        // Silenzioso - non mostrare errori all'utente
        console.debug('Google Ping Scheduler:', error.message);
    });
}
