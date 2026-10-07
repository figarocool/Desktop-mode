# Desktop Mode — guida del sistema

Desktop Mode è un ambiente desktop per PS Vita scritto in C con VitaSDK. La shell offre desktop con icone spostabili, menu Start, menu contestuali, taskbar e finestre ridimensionabili. Il puntatore può essere controllato con gli stick, touch o dispositivi HID supportati. Le app hanno finestre e voci nella taskbar; possono essere minimizzate, ripristinate, massimizzate e chiuse.

## Architettura e app

La shell nativa gestisce input, finestre, memoria, filesystem e servizi di sistema. I moduli `.dmapp` possono contenere un'app WASM eseguita dal runtime WebAssembly oppure un modulo nativo Vita. Le app WASM usano le API host del Desktop SDK e sono isolate nel runtime; i moduli nativi condividono il processo della shell e richiedono fiducia maggiore. Le app non sono processi Vita separati.

Le app integrate sono Notepad, Browser, Calcolatrice, Paint, Anteprima immagini, PDF, Lettore multimediale, Console, Task Manager, Rete, Solitario, Campo minato e Contatore. App esterne possono registrarsi tramite SDK e `.dmapp`; la shell non va ricompilata per installarle. Il riferimento completo alle funzioni e ai controlli è in [API Desktop Mode, italiano](desktop-api.md) e [API Desktop Mode, English](desktop-api-en.md).

## File e associazioni

Risorse del computer mostra le unità rilevate, tra cui `ux0:`. Explorer supporta navigazione, selezione multipla, copia, spostamento, rinomina, eliminazione con cestino e trascinamento. Le operazioni lunghe mostrano lo stato di avanzamento. Le associazioni permettono di aprire un'estensione con un'app installata; immagini, testo e PDF hanno app integrate.

## App integrate

Notepad modifica testo con selezione e copia/incolla. Paint consente di disegnare e modificare la tela, anche oltre l'area visibile tramite scrollbar. Anteprima immagini apre i formati raster supportati dal decoder integrato. Il visualizzatore PDF usa la libreria inclusa nel progetto; i limiti sono in [PDF](pdf-viewer.md).

Il Browser usa il motore interno: parsing HTML5/CSS con Lexbor, JavaScript con Duktape, immagini PNG/JPEG/WebP, moduli, schede, cronologia e preferiti. Il layout e le API web supportate sono parziali e non equivalgono a un browser desktop completo. Il Lettore multimediale riproduce i formati disponibili tramite decoder integrati o backend della piattaforma; [dettagli e limiti](media-player.md).

La Console include comandi per file e directory. Task Manager mostra finestre e memoria tracciata dalle app compatibili. Solitario e Campo minato sono giochi WASM. Il Pannello di controllo gestisce personalizzazione, lingua, impostazioni, app installate e dispositivi.

## Rete, Bluetooth e desktop remoto

La rete locale può rilevare host IPv4 e aprire condivisioni SMB2/3 con navigazione e trasferimento di file singoli. L'interfaccia Wi-Fi può mostrare reti e inviare richiesta di connessione con password. Il supporto di DHCP/IP statico e alcune funzioni dipendono dalle API e dal firmware e richiedono collaudo su console. Bluetooth usa API userland della Vita per la ricerca e l'associazione dei dispositivi supportati.

Il server Desktop remoto condivide la schermata della shell con mouse e tastiera remoti. L'implementazione RDP è parziale: NLA/CredSSP, audio e clipboard remota non sono disponibili; le connessioni reali via Wi-Fi vanno collaudate. Vedere [RDP](remote-desktop.md) e [stato rete e USB](status-network-usb.md).

## Aggiornamenti

All'avvio la Vita controlla l'ultima release GitHub. Scarica e verifica le `.dmapp` modificate, poi le carica da `ux0:/data/desktop-mode/apps/` dando loro precedenza sulle copie nel VPK. Se trova un core semanticamente più nuovo, verifica ed estrae la VPK e tenta l'installazione con il package promoter nativo; se riesce, chiude l'app affinché il lancio successivo usi il core aggiornato. Se la console rifiuta l'installazione, conserva la VPK verificata in `ux0:/data/desktop-mode/updates/desktop-mode.vpk`. Il manifest e il flusso di release sono descritti in [Aggiornamenti](aggiornamenti-release.md).

## Compilazione e test

Su Linux, `./scripts/desktop-preview.sh` compila il preview desktop. Per la Vita è richiesto VitaSDK e CMake, poi si compila il target `desktop-mode.vpk-vpk` in `native/build`. I test principali sono `bash scripts/test-wasm-app-runtime.sh` e `bash scripts/test-widgets.sh`. Il preview non riproduce tutte le API hardware Vita: Wi-Fi, Bluetooth, input e installazione VPK richiedono test sulla console.
