# Desktop Mode App per PS Vita

Homebrew in C / vita2d, 960×544, title ID DSKMODE01. Installare `build/desktop-mode.vpk` con VitaShell su una console abilitata agli homebrew. Compilazione Vita riuscita; comportamento verificato sul backend Linux, non ancora su hardware Vita.

## Desktop

Icone trasparenti ricreate dal riferimento Vista / Play OS. Un clic seleziona, due clic entro 450 ms aprono. Tenere premuto e trascinare sposta le icone del desktop; le posizioni vengono ricordate. Esplora risorse evidenzia il file o la cartella selezionati e mostra il nome nello stato.

Fino a otto finestre, con pulsante nella taskbar per ogni istanza, chiusura, minimizzazione e massimizzazione/ripristino. Il pannello di controllo mostra modello, firmware, batteria, clock CPU/GPU, RAM libera, spazio ux0, IP disponibile e moduli caricati. I valori Vita non vengono simulati nell'anteprima Linux.

Tre sfondi inclusi e sfondo personalizzato PNG/JPG, con percorso salvato e ripristinato all'avvio. L'immagine deve restare nel percorso salvato. Immagini molto grandi possono superare la memoria disponibile.

## Esplora risorse

Computer enumera le unità accessibili tra ux0, uma0, imc0, ur0, ud0, vs0, os0, sa0, gro0, grw0. Doppio clic sull'unità mostra cartelle e file reali; doppio clic su una cartella la apre. Documenti apre ux0:/data; Giochi apre ux0:/app senza eseguire giochi nella shell.

Menu contestuale sull'elemento: Apri, Copia, Elimina, Crea collegamento sul desktop. Nella cartella: Incolla, Nuovo file di testo, Nuova cartella, Aggiorna. Sul desktop: file/cartelle/collegamenti persistenti, incolla, personalizzazione e ricerca di nuove app. Il desktop conserva i suoi file in ux0:/data/desktop-mode/Desktop.

Copia file e cartelle senza sovrascrivere; usa nomi alternativi mantenendo l'estensione. Elimina chiede conferma e sposta nel Cestino della stessa unità; dal Cestino si può ripristinare o svuotare con conferma. Copia e svuotamento avanzano a blocchi con barra, dimensione, tempo trascorso, stima restante e annullamento. Le copie già completate restano dopo annullamento; le eliminazioni effettuate durante lo svuotamento sono definitive. Taglia dei file non è ancora implementato; Rinomina è disponibile nei menu. Scritture limitate alle destinazioni consentite dall'API.

## App esterne

Notepad, Browser e Contatore sono moduli compilati separatamente dal desktop. Nuove app `.dmapp` copiate in ux0:/data/desktop-mode/apps si agganciano a runtime e compaiono in Start, senza ricompilare il desktop. Il pulsante Cerca nuove app del pannello le carica anche durante la sessione. Le app dichiarano le proprie estensioni e ricevono il percorso del file da aprire.

Il caricamento è indipendente: un pacchetto mancante, incompatibile o con errore di avvio viene segnalato e la scansione continua con le altre app. I moduli `.dmapp` però condividono il processo Vita di Desktop Mode; un crash durante l'esecuzione può quindi terminare anche il desktop. Per isolare davvero i crash occorre avviare ogni app in un processo Vita distinto, il che cambia l'esperienza delle finestre integrate e va deciso prima di implementarlo.

Notepad apre TXT/INI/LOG/JSON/MD, modifica e salva, usa finestre comuni Apri/Salva con nome per scegliere Desktop o cartelle delle unità, offre selezione parziale, taglia/copia/incolla e seleziona tutto e protegge le modifiche non salvate alla chiusura. Limite 32767 byte; file più grandi vengono rifiutati per evitare troncamenti. Supporta trascinamento del testo, Shift+frecce, Ctrl+A/C/X/V e sostituzione della selezione durante la scrittura. Sulla Vita: posizionare il cursore, premere Seleziona e indicare l’altro estremo con X, oppure trascinare con X tenuto o touch. Tastiera inserisce il testo al cursore o sostituisce la selezione. Non implementa ancora annulla/ripeti. Contatore e Hello sono esempi per l'SDK. Word/DOCX, musica, messaggistica e server RDP non sono implementati. Browser usa Lexbor per analizzare HTML5/CSS e Duktape per JavaScript isolato; supporta immagini PNG/JPEG/WebP, moduli GET/POST, schede, cronologia e preferiti. Il layout CSS è parziale, senza un albero completo di box: limiti e API coperte sono descritti in [native/vendor/browser-engine/README.md](vendor/browser-engine/README.md).

## Controlli

Vita: stick sinistro muove il puntatore. X seleziona, doppio X apre, tenere X premuto e muovere lo stick trascina le icone del desktop o la barra del titolo. Touch equivalente. Quadrato apre il menu contestuale. START apre Start; Cerchio annulla/torna indietro; L/R scorrono elenco, testo o Start. Pulsanti finestra: `_` minimizza, `[]` massimizza/ripristina, `X` chiude. Notepad usa il pulsante Tastiera per modificare il testo con tastiera interna.

Linux: clic/doppio clic/trascinamento e tasto destro. F1 apre Start, Escape corrisponde a Cerchio, PageUp/PageDown a L/R. In Notepad: tastiera fisica, Invio, Backspace, frecce, Ctrl+S, Ctrl+A seleziona tutto, Shift+frecce/Home/End estende la selezione, Ctrl+C copia la selezione, Ctrl+X taglia, Ctrl+V. La tastiera interna funziona anche qui.

## Compilazione

```sh
VITASDK=/usr/local/vitasdk cmake -S native -B native/build
VITASDK=/usr/local/vitasdk cmake --build native/build -j2
./scripts/desktop-preview.sh
SDL_VIDEODRIVER=dummy DESKTOP_SELF_TEST=1 ./scripts/desktop-preview.sh
```

Vita: VitaSDK, vita2d, libpng/libjpeg. Linux: compilatore C, SDL2, SDL2_image, SDL2_ttf. [SDK e sviluppo indipendente](../docs/desktop-api.md).

Test: file/cartelle, protezione da sovrascrittura, copia ricorsiva, collegamenti, salvataggio e selezione UTF-8/appunti Notepad, doppio clic, trascinamento, massimizzazione, caricamento runtime di un modulo separato, associazioni e aggiornamenti mentre minimizzato. Verificati anche con AddressSanitizer/UBSan. Il collaudo Vita resta necessario per caricamento SUPRX, touch, grafica, unità e informazioni hardware.

## Orologio e batteria

Etichette delle icone centrate sulla larghezza effettiva del testo. Batteria accanto all’orologio, con animazione durante la carica. Un clic sull’orologio apre calendario e impostazioni per mostrare la data e regolare data/ora del desktop. Questa regolazione non modifica l’orologio globale della console; quello va impostato nelle impostazioni Vita.

Le verifiche Linux includono il selettore condiviso, salvataggio sul Desktop, copia a blocchi di 2 MiB e annullamento, eliminazione con conferma, ripristino senza sovrascrittura e svuotamento su un’unità di prova isolata.

## Browser e app installate

L’icona Internet apre Browser. Nell’anteprima Linux cliccare la barra bianca, digitare e premere Invio oppure Vai. Ctrl+A/C/X/V e frecce funzionano anche nella barra. Sulla Vita il clic apre la tastiera interna; confermare l’indirizzo. Preferiti elenca quelli salvati; Aggiungi conserva l’indirizzo corrente. HTTPS verifica i certificati con il bundle CA incluso. Limiti runtime: 8 MiB per documento o risorsa scaricata, 512 KiB di CSS esterno combinato, 256 KiB per script esterno, 320 righe visualizzate, 96 link, 16 voci di cronologia e 32 preferiti. La suite runtime verifica inoltre un documento HTML da 700 KiB.

Pannello → Applicazioni installate permette installazione, disinstallazione confermata e ripristino delle app incluse. Browser e Notepad sono rimovibili da Start e dalle associazioni anche dopo riavvio. Le app incluse restano materialmente nel VPK di sola lettura; quelle installate dall’utente vengono rimosse dalla memoria. Documenti e preferiti vengono conservati. Chiudere tutte le finestre dell’app prima di disinstallarla. Riavviare per sostituire il codice di un modulo già caricato.

Per diagnosticare app non caricate, aprire `ux0:/data/desktop-mode/app-loader.log` con Esplora risorse o VitaShell e condividere il file. Ogni nuova scansione sostituisce il log con l'esito completo dei moduli trovati; installazioni manuali successive vengono aggiunte in fondo.

## Rete locale e Samba

L’icona Rete apre una finestra nativa: Scansiona controlla la sottorete IPv4 locale, fino al segmento /24, con 24 tentativi contemporanei e timeout di 400 ms. Mostra gli IP che rispondono sulle porte 445/139/80/443/22; segnala SMB quando 445 è aperta. Non identifica il tipo di dispositivo e non garantisce di trovare host nascosti da firewall. La scansione parte solo premendo Scansiona.

Doppio clic su un IP tenta l’elenco delle condivisioni SMB2/3 tramite IPC$. Connetti permette anche IPv4/condivisione quando l’elenco non è disponibile. Credenziali imposta utente e password solo in memoria; la password è mascherata nella tastiera e cancellata su Stop/chiusura. Doppio clic apre cartelle condivise; Su torna indietro. Scarica trasferisce un file a blocchi nel percorso scelto e lo apre con l’app associata. Invia carica un file scelto con il selettore condiviso del sistema, mostrando l’avanzamento; crea un temporaneo remoto esclusivo e lo pubblica col nome originale solo a trasferimento completato. Se si interrompe la connessione, il temporaneo può restare nella condivisione. Elimina chiede conferma e rimuove definitivamente il file remoto; le cartelle devono essere vuote. Rinomina evita nomi già esistenti e Cartella crea nuove directory. Sono gestiti file singoli, non upload/download ricorsivi. SMB1 non è implementato.

I test offline verificano righe/IP, riconoscimento SMB, input invalido, protezione dei file esistenti nei download, conferma e annullamento del trascinamento nel Cestino dal desktop e da Explorer. Il sandbox di sviluppo blocca i socket locali: HTTP e Samba reali non sono stati collaudati in questo ambiente, e il test sulla console rimane necessario.

L’icona del Cestino conserva il contenitore originale: mostra fogli quando contiene elementi e torna vuota dopo svuotamento/ripristino dell’ultimo elemento. Lo stato viene aggiornato a fine operazione e periodicamente per modifiche esterne.

Rinomina è disponibile nei menu del Desktop e di Explorer, nel selettore Apri/Salva e nelle condivisioni SMB. Non sovrascrive nomi esistenti. Notepad segue le rinomine di documenti e cartelle prima di salvare. La rinomina locale è sospesa durante operazioni di copia/Cestino.

La shell e le app sono native C. Gli attuali pacchetti `.dmapp` richiedono una compilazione per piattaforma; un futuro runtime a bytecode potrebbe consentire app universali. PSP/PS3 e questo runtime non sono implementati. Vedere `docs/architecture.md` nel progetto.

## Nuove app separate

Il VPK include quattro ulteriori pacchetti esterni: `calculator.dmapp` (operazioni +, -, ×, ÷, decimali, cambio segno, copia risultato), `taskmanager.dmapp` (elenco finestre, mostra/minimizza/chiudi e RAM libera), `paint.dmapp` (tela 560×240, pennello a tre dimensioni, gomma, otto colori, annulla/ripeti dell'ultima operazione e salvataggio PNG) e `images.dmapp` (apertura PNG/JPEG e adattamento alla finestra). Sono disponibili in Start e nel pannello delle applicazioni installate.

Il doppio clic su PNG/JPEG apre Anteprima immagini tramite associazione. Per uno sfondo personalizzato usare Personalizzazione → Scegli sfondo personalizzato. Paint crea disegni nuovi; questa prima versione non importa immagini esistenti. Task Manager gestisce le finestre interne di Desktop Mode, non i processi Vita.

## Console separata

`console.dmapp` compare in Start come Console. Su Linux si può digitare direttamente nel campo inferiore ed eseguire con Invio; sulla Vita un clic/X sul campo apre la tastiera condivisa e confermare esegue il comando. Il pulsante Esegui usa il testo già inserito. Su/Giù richiamano la cronologia; L/R o i pulsanti Registro scorrono l'output. `help` elenca i comandi e i limiti. Usa unità come `ux0:/` anziché lettere Windows. Esempio: `cd ux0:/data`, `dir`, `mkdir "Nuova cartella"`, `cp "Documento.txt" "Copia.txt"`. `del`/`rm` chiedono conferma e spostano nel Cestino; `mv`/`ren` rinominano nella stessa directory.

## PDF e adattamento Paint

Paint adatta la tela a tutta l'area della finestra e converte il puntatore nelle coordinate del disegno, anche dopo massimizzazione/ripristino. La risoluzione del documento salvato rimane 560×240.

Il visualizzatore PDF è un modulo separato incluso nel VPK e nell'anteprima. Usa il renderer C del progetto `libdesktop_pdf.a`, con apertura `.pdf`, pagine, zoom e scorrimento. Supporta testo, grafica e immagini nei formati documentati; la compatibilità PDF è parziale, con errori espliciti per caratteristiche non supportate. Dettagli e verifiche in [PDF](../docs/pdf-viewer.md).

## Selezione multipla, volume e Bluetooth

Desktop e Explorer supportano il rettangolo di selezione trascinando da uno spazio vuoto, Ctrl+clic per aggiungere/togliere, Shift+clic per un intervallo, Ctrl+A e Seleziona tutto nei menu. In Explorer il rettangolo puo partire anche dal margine destro della lista. Sulla Vita SELECT+X permette di aggiungere/togliere elementi con joypad; X tenuto sposta il gruppo di icone selezionate. Il doppio clic continua ad aprire un elemento. Copia/Incolla ed Elimina operano su tutto il gruppo; Elimina chiede una conferma e usa il Cestino. L'annullamento ferma la coda mantenendo le operazioni gia completate. Una copia con nome esistente crea un nome `- copia N`, senza sovrascrivere. Rinomina richiede un solo elemento.

Lo speaker nella taskbar apre il controllo 0-100% e disattiva/riattiva l'audio. Su Vita usa il volume di sistema SceAVConfig (0-30); in Linux il valore e simulato e non modifica il mixer del computer. L'icona Bluetooth apre lo stesso pannello disponibile dal Pannello di controllo, con ricerca, elenco dispositivi, associazione, PIN/conferma e disconnessione tramite API native userland SceBt. Il Bluetooth deve essere attivo nelle impostazioni della console. Nessun plugin di sistema aggiuntivo. Tastiera italiana con AltGr e ripetizione; mouse tramite SceHid. Codice compilato e testato con backend simulati; associazione reale, volume e compatibilità richiedono ancora prova su Vita. [Dettagli e limiti](../docs/selection-and-devices.md).
