# Desktop Mode App

Desktop PS Vita scritto in C, ispirato a Vista / Play OS: icone trasparenti e spostabili, menu Start e menu contestuali, file manager, finestre con taskbar e pannello di controllo. Interfaccia nativa, senza JavaScript o PocketJS.

- VPK: `native/build/desktop-mode.vpk`.
- Anteprima Linux: `./scripts/desktop-preview.sh`.
- [Funzioni, controlli e limiti](native/README.md).
- [API e SDK per app esterne (italiano)](docs/desktop-api.md) · [API and SDK (English)](docs/desktop-api-en.md).
- [Guida completa del sistema (italiano)](docs/guida-sistema-it.md) · [System guide (English)](docs/system-guide-en.md).
- [Release e aggiornamenti automatici](docs/aggiornamenti-release.md).

Le app si sviluppano e compilano separatamente: copiare i pacchetti `.dmapp` in `ux0:/data/desktop-mode/apps/` e selezionare Cerca nuove app. Notepad, Browser e Contatore sono già moduli esterni al desktop; l'SDK include Hello come esempio con associazione `.hello`. La shell non deve essere ricompilata per aggiungerli.

La guida del sistema è disponibile in [italiano](docs/guida-sistema-it.md) e [inglese](docs/system-guide-en.md). Il codice compila per Linux e Vita; alcune funzioni hardware e la rete reale richiedono comunque test sulla console.

Notepad supporta selezione parziale e taglia/copia/incolla. Browser usa il renderer interno nella finestra del desktop: parser HTML5/CSS Lexbor, JavaScript Duktape isolato, immagini PNG/JPEG/WebP, moduli GET/POST, schede, link e preferiti. Il layout CSS resta parziale e non equivale a un browser desktop; dettagli e limiti sono in [documentazione del renderer](native/vendor/browser-engine/README.md). Rete cerca IP locali e apre condivisioni SMB2/3. Le app incluse sono disinstallabili dal pannello. File e cartelle possono essere trascinati sul Cestino con conferma.

PDF ora incluso nel VPK tramite la libreria C del progetto, con compatibilità parziale: [supporto e test PDF](docs/pdf-viewer.md). Bluetooth usa le API native userland per ricerca/associazione; tastiera italiana con ripetizione. [Stato dei dispositivi e limiti di collaudo](docs/selection-and-devices.md).

Start → **Esegui...** apre cartelle e documenti, oppure avvia app installate e pacchetti `.dmapp`. Il server **Desktop remoto RDP** si abilita dal Pannello di controllo con una password e mostra IP e porta. Condivide il desktop dell'app a 960×544 con mouse e tastiera. Implementazione iniziale TLS, senza NLA/CredSSP, audio o clipboard remota; protocollo verificato con FreeRDP 3 via trasporto in memoria. Connessione TCP/Wi-Fi su Vita e client Windows ancora da verificare. [Istruzioni RDP](docs/remote-desktop.md).

Icone dei documenti, miniature PNG/JPG, programmi predefiniti configurabili e correzione dell’anteprima screensaver: [uso e limiti](docs/file-types-and-screensaver.md).

La finestra Rete cerca dispositivi IPv4 e gestisce condivisioni SMB: navigazione, download e invio file, rinomina, nuove cartelle ed eliminazione con conferma. Il pannello console mostra i dettagli dell’adattatore, cerca access point e invia SSID/password alle API userland private ricostruite; scansione e connessione vanno collaudate su Vita, mentre DHCP/IP statico restano da implementare: [stato rete e limiti](docs/status-network-usb.md).

La lingua IT/EN/ES si applica all’interfaccia integrata e ai moduli inclusi. Le app create con Desktop API possono usare `dm_localize(it, en, es)` per le proprie traduzioni; `dm_text_raw` conserva testo, nomi e percorsi digitati senza tradurli.

All'avvio, il sistema controlla le release GitHub e aggiorna i moduli `.dmapp` verificati; un nuovo VPK del core viene scaricato e messo in staging per l'installazione tramite Vita. Vedi [documentazione aggiornamenti](docs/aggiornamenti-release.md).
