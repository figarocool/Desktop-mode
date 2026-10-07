# Desktop API v3 — app esterne in C

English reference: [desktop-api-en.md](desktop-api-en.md). The two references cover the public native C plug-in API and the WebAssembly app ABI.

Le app sono compilate separatamente dal desktop e caricate a runtime nello stesso processo: pacchetti `.dmapp` specifici per Vita oppure Linux. Non occorre aggiungere il loro sorgente al desktop, modificarne il menu o ricompilare la shell. Notepad, Browser e Contatore inclusi nel VPK sono già moduli separati.

## Sviluppare un'app

Includere `desktop_plugin.h`, definire un descrittore `DmApp` e terminarlo con `DM_EXPORT_APP(nome_descrittore)`. L'SDK fornisce le funzioni attraverso una tabella di puntatori passata dal desktop: l'app non deve linkare il suo eseguibile.

```c
#include "desktop_plugin.h"
#include <stdio.h>
typedef struct { char file[DM_PATH_MAX]; } State;
static void open_app(DmWindow *w, const char *argument) {
    snprintf(((State *)w->state)->file, DM_PATH_MAX,
             "%s", argument ? argument : "Nessun file");
}
static void draw(DmWindow *w) {
    dm_text(w->x + 20, w->y + 80,
            ((State *)w->state)->file, DM_COLOR(24, 38, 55, 255));
}
const DmApp my_app = {
    DM_API_VERSION, "my-app", "La mia app", sizeof(State),
    open_app, draw, NULL, NULL, NULL, NULL,
    ".mydoc,.example", NULL
};
DM_EXPORT_APP(my_app)
```

Le estensioni sono dichiarate nel modulo. L'apertura di `file.mydoc` lancia `my-app` passando il percorso a `open_app`. L'app deve poi leggere e interpretare il contenuto: l'associazione non converte automaticamente i formati.

Esempio completo: `native/examples/hello.c` apre l'estensione `.hello` e riceve aggiornamenti anche quando minimizzato.

## Compilazione indipendente

Dalla cartella del progetto:

```sh
./scripts/build-app.sh linux /percorso/mia-app.c mia-app
./scripts/build-app.sh vita /percorso/mia-app.c mia-app
```

Lo script compila soltanto il modulo. Su Linux copia il risultato nella cartella app dell'anteprima. Su Vita il risultato è `native/build/plugins/vita-mia-app/mia-app.dmapp`, da copiare in:

```text
ux0:/data/desktop-mode/apps/
```

L'app viene scoperta all'avvio, oppure dal pulsante **Cerca nuove app** del pannello di controllo o dal menu desktop **Aggiorna / cerca app**. Compare automaticamente in Start. Doppio clic su un `.dmapp` in Esplora risorse lo carica per la sessione; per ritrovarlo all'avvio va conservato nella cartella app. Anche Linux usa `.dmapp`, contenenti un modulo Linux. Non sono intercambiabili tra le piattaforme.

Il pacchetto SDK distribuibile contiene `desktop_api.h`, `app-sdk/desktop_plugin.h`, il CMake indipendente, `package-app.py` e un esempio. Da un SDK estratto si può compilare senza il repository del desktop:

```sh
cmake -S app-sdk -B build -DDM_APP_LINUX=ON \
  -DDM_APP_SOURCE=/percorso/mia-app.c -DDM_APP_NAME=mia-app
cmake --build build
```

Per Vita, impostare `VITASDK` e usare `-DDM_APP_LINUX=OFF`. Il linker dei moduli usa SceLibc/SceLibKernel, senza lo startup dell'eseguibile principale.

## API

| API | Funzione |
| --- | --- |
| `dm_launch`, `dm_focus`, `dm_close`, `dm_maximize` | Finestre e ciclo di vita |
| `dm_rect`, `dm_text`, `dm_text_width`, `dm_text_center` | Disegno con coordinate assolute nel desktop 960×544; `dm_text` traduce automaticamente le etichette presenti nel catalogo UI |
| `dm_text_raw`, `dm_text_width_raw` | Testo utente non tradotto: nomi file, percorsi, contenuto dei documenti o risultati dei comandi |
| `dm_localize(it, en, es)`, `dm_ui_translate(it)` | Traduzione esplicita per stringhe dell'app e riuso del catalogo UI IT/EN/ES |
| `dm_prompt` | Tastiera interna con risultato asincrono, annullamento senza callback |
| `dm_clipboard_text_set/get` | Appunti di testo condivisi |
| `dm_fs_read/write/copy` | File e cartelle con percorsi delle unità Vita |
| `dm_fs_join/parent/is_directory/writable` | Gestione dei percorsi |
| `dm_associate_extension`, `dm_open_file` | Associazioni e apertura attraverso il desktop |
| `dm_system_info`, `dm_clock_ms` | Informazioni console e orologio monotono |

### API delle app WebAssembly

Le app WASM includono `native/app-sdk/wasm_app.h`, importano funzioni dal modulo host `desktop` ed esportano almeno `dm_app_abi_version`, `dm_app_init`, `dm_app_draw`, `dm_app_click` e `dm_app_close`. Ogni finestra riceve un runtime e una memoria propri; il desktop controlla puntatori e capacità dei buffer. Le coordinate sono locali all'area client. Gli handle di immagini e servizi sono interi opachi, non puntatori Vita.

| Gruppo | Import | Uso |
| --- | --- | --- |
| Grafica e testo | `rect`, `scrollbar_draw`, `text`, `text_scaled`, `text_width`, `text_width_scaled` | Disegnare forme e testo e misurare le etichette con il font configurato. |
| Puntatore e attività | `pointer_state`, `task_count`, `task_get`, `task_memory`, `task_action` | Leggere il puntatore e consultare o gestire le finestre di Desktop Mode. |
| Immagini | `image_load`, `image_size`, `image_read`, `image_draw`, `image_draw_clipped`, `image_free`, `image_create`, `image_update`, `image_save_png` | Caricare, misurare, leggere e modificare pixel, disegnare e salvare PNG. La capacità di lettura è in byte. |
| File system | `fs_list_text`, `fs_is_directory`, `fs_mkdir`, `fs_copy`, `fs_rename`, `fs_path_update`, `fs_read`, `fs_write` | Elencare cartelle, verificare e modificare file tramite chiamate host limitate. |
| Dialoghi | `file_dialog`, `file_dialog_at`, `text_prompt`, `confirm`, `open_file` | Selezionare file, chiedere testo o conferma e aprire file tramite le associazioni del desktop. I risultati arrivano ai callback dell'app. |
| Appunti e avvio | `clipboard_set`, `clipboard_get`, `launch` | Condividere testo e avviare un'altra app registrata. |
| Operazioni asincrone | `copy_async`, `trash_async` | Avviare copia o spostamento nel Cestino e ricevere l'esito con l'evento dell'app. |
| Rete e browser | `network_info`, `network_row`, `network_action`, `network_select`, `http_start`, `http_post`, `http_poll`, `http_error`, `http_url`, `js_eval`, `js_output` | Leggere e gestire il servizio di rete e usare le funzioni HTTP e JavaScript con buffer limitati. Il corpo HTTP massimo è `DM_WASM_HTTP_MAX_BODY`. |
| PDF | `pdf_open`, `pdf_close`, `pdf_render` | Aprire un PDF e renderizzare una pagina in un buffer pixel fornito dall'app. |
| Multimedia | `media_open`, `media_action`, `media_status`, `media_time_ms`, `media_duration_ms`, `media_seek_ms`, `media_set_volume`, `media_get_volume`, `media_set_speed`, `media_metadata` | Controllare il player host e leggere stato, posizione, volume, velocità e metadati. |

Le importazioni sono capacità fornite dal desktop, non chiamate dirette al kernel. Quelle opzionali possono mancare su host precedenti: controllare gli errori e prevedere un comportamento alternativo. Le app preinstallate WASM usano l'estensione `.dmapp`; l'header identifica il tipo di payload. I plugin nativi di terze parti continuano a usare `.dmapp`, mentre `.suprx` identifica un modulo kernel Vita.

## Controlli grafici e confronto con Windows 95

Le API primitive (`dm_rect`, `dm_text`, input e scrollbar) permettevano già di disegnare controlli, ma non fornivano oggetti riutilizzabili. `native/app-sdk/dm_widgets.h` aggiunge un toolkit immediato in stile classico: includerlo dopo `desktop_plugin.h` per un'app C nativa oppure definire `DM_WIDGETS_WASM` e includerlo dopo `wasm_app.h`. Le funzioni di disegno sono separate dagli helper `*_click`: l'app conserva selezioni e valori nel proprio stato, chiama gli helper dal callback `click` e ridisegna dal callback `draw`. Il toolkit non possiede controlli persistenti e lascia all'app l'editing del testo, lo sorting, il caricamento delle pagine e l'esecuzione dei comandi. Verificarlo con `sh scripts/test-widgets.sh`. Tutti i 13 moduli WASM preinstallati includono ora questo header e usano i controlli condivisi per i pulsanti, i campi editabili e/o le righe elenco; le aree specifiche delle app (canvas, carte, pagina HTML e testo selezionabile) mantengono il proprio rendering e stato.

| Controllo Win95 | Copertura precedente | Toolkit Desktop Mode | Opzioni e comportamento |
| --- | --- | --- | --- |
| Static / label | Disegnabile con testo libero | `dmw_label` | Testo localizzato o grezzo, colore |
| Push/default button | Disegnato manualmente dalle app | `dmw_button`, `dmw_button_click` | Disabilitato, predefinito, premuto |
| Checkbox / radio | Disegnati manualmente in alcune app | `dmw_checkbox`, `dmw_radio` e helper click | Stato checked; radio con valore di gruppo; disabilitato |
| Group box | Nessun helper | `dmw_groupbox` | Cornice con titolo |
| Edit / text box | Prompt condiviso e input specifici per app | `dmw_edit`, `dmw_edit_click` | Read-only, password, multilinea, focus/cursore. L'editing e la selezione del testo restano gestiti dall'app, che riceve gli eventi testo/tastiera |
| List box | Elenchi disegnati caso per caso | `dmw_listbox_ex`, `dmw_listbox_metrics`, `dmw_listbox_click_ex`, `dmw_listbox_scroll_click`, `dmw_listbox_select` | Selezione singola o multipla fino a 32 elementi, scrollbar verticale e orizzontale automatiche con hit testing; ordinamento a carico dell'app |
| Combo box | Nessun helper | `dmw_combo_ex`, `dmw_combo_ex_click` | Tendina selezionabile con apertura/chiusura e scelta voce; per la variante editabile si compone con `dmw_edit` |
| Scroll bar | Già disponibile | `dm_scrollbar_draw` / `dm_host_scrollbar_draw` | Verticale/orizzontale, proporzionale al contenuto |
| Toolbar | Nessun oggetto condiviso | `dmw_toolbar_button`, `dmw_icon_button`, `dmw_icon_button_click` | Pulsanti testuali o icone standard di trasporto (play/pausa/stop, traccia, rewind/forward, volume e playlist); stato premuto/disabilitato, layout e comandi gestiti dall'app |
| Status bar | Nessun helper | `dmw_statusbar` | Parti multiple con larghezza adattata al testo |
| Progress bar | Nessun helper | `dmw_progress` | Intervallo minimo/massimo, riempimento deterministico |
| Trackbar / slider | Nessun helper | `dmw_trackbar`, `dmw_trackbar_click` | Intervallo numerico e aggiornamento cliccando la traccia |
| Up-down / spin | Nessun helper | `dmw_spin`, `dmw_spin_click` | Incremento/decremento con limiti |
| Tab | Menu bar e pagine app, senza controllo tab riutilizzabile | `dmw_tabs`, `dmw_tabs_click` | Selezione della scheda; l'app disegna il pannello corrispondente |
| Menu bar / popup | Disegnati dalla shell o dalle singole app | `dmw_menubar`, `dmw_menubar_click`, `dmw_menu_popup`, `dmw_menu_popup_click` | Etichette a larghezza dinamica, voce selezionata e voci disabilitate; comando gestito dall'app |
| Hit testing scrollbar | Disegno condiviso, hit testing manuale | `dmw_scrollbar_click` | Aggiorna offset proporzionale; l'app conserva il valore e gestisce il trascinamento continuo |
| Header e list-view | Esplora risorse implementato su misura | `dmw_header`, `dmw_listview_row` | Colonne e righe; sorting, resize e ordinamento dei dati spettano all'app |
| Tree-view | Nessun controllo generico; Explorer ha una navigazione dedicata | `dmw_treeview`, `dmw_treeview_metrics`, `dmw_treeview_click`, `dmw_treeview_toggle_click`, `dmw_treeview_scroll_click` | Nodi gerarchici con profondità, selezione, espansione e scrollbar verticale/orizzontale; l'app mantiene la struttura e passa i nodi visibili in ordine |
| Tooltip | Nessun helper | `dmw_tooltip` | Disegno del riquadro; l'app decide quando mostrarlo |
| Hot-key / animation | Nessun helper | `dmw_hotkey`, `dmw_animation_frame` | Visualizzazione di una scorciatoia e frame/progresso semplice; cattura scorciatoie e decodifica AVI non incluse |

Il toolkit copre i tipi di controllo standard e common control citati nella tabella, ma non replica ogni opzione e messaggio Win32: il modulo resta immediato, senza finestre figlie o `WndProc`; il codice dell’app conserva i dati e gestisce le operazioni avanzate. In particolare il controllo animate disegna un indicatore, non decodifica AVI; hot-key visualizza la scorciatoia senza cattura automatica; list-view fornisce celle/righe ma non ogni modalità/icon list o sorting. Il toolkit fornisce rendering e hit-testing, non classi Win32 binariamente compatibili: non importa `CreateWindow`, `WndProc`, messaggi `WM_*` o `COMCTL32`. Funzioni avanzate come drag di colonne, ordinamento, immagini associate, accessibilità automatica e decoder di animazioni restano responsabilità dell'app o non sono ancora implementate. Il riferimento Microsoft elenca i controlli intrinseci (button, edit, static, listbox, combobox e scrollbar) e i common controls Win95 (toolbar, status bar, trackbar, progress, tab, tooltip, list-view, tree-view, header, up-down, hot-key e animate): [controlli intrinseci](https://learn.microsoft.com/en-us/windows/win32/menurc/control-control), [classi common controls Win95](https://learn.microsoft.com/en-us/windows/win32/api/commctrl/ns-commctrl-initcommoncontrolsex), [guida UI Windows 95](https://www.bitsavers.org/pdf/microsoft/windows_95/Programming_the_Windows_95_User_Interface_1995.pdf).

Il descrittore specifica `open`, `draw`, `click`, `text`, `key`, `close`, `extensions` e `tick`. La shell alloca a zero lo stato di ciascuna finestra. `click` riceve coordinate relative; `text` riceve UTF-8. `close` può restituire 0 per annullare la chiusura. Le risorse dell'app vanno liberate quando il suo `close` restituisce 1; lo stato viene poi liberato dalla shell.

`tick` continua per tutte le istanze aperte, anche minimizzate e dietro altre finestre. Il tempo trascorso è limitato a 250 ms per aggiornamento. Audio e rete richiedono codice e dipendenze della singola app; per elaborazioni lunghe serve lavoro asincrono, perché un callback bloccante ferma temporaneamente il desktop.

Limiti v3: otto finestre, 24 app registrate, 24 moduli e 64 associazioni. Se più app dichiarano la stessa estensione, prevale l'ultima registrata, salvo una scelta esplicita dell'utente in Pannello di controllo → Tipi di file. Le preferenze vengono salvate in `ux0:/data/desktop-mode/associations.ini`; la registrazione delle app non sovrascrive queste scelte. Se l'app scelta non è installata, viene usata l'associazione disponibile; reinstallandola torna valida la scelta dell'utente. Il menu Start scorre con L/R quando ci sono più di 8 app. `.dmapp`, `.dmsaver` e `.dmlink` hanno gestione interna riservata e non possono essere associate da un'app o dall'utente. PNG/JPG/JPEG usano il visualizzatore di immagini per default e possono essere riassegnate, come TXT e PDF. Le app condividono il processo, hanno accesso nativo e non sono isolate. I moduli caricati restano residenti fino all'uscita; per sostituire un modulo già caricato occorre riavviare il desktop. API/versione/dimensione della tabella sono controllate all'ingresso.

## File e collegamenti

Copia di file e cartelle senza sovrascrivere: in caso di collisione viene usato ` - copia N` mantenendo l'estensione. La copia ricorsiva ha un limite di 32 livelli. Errori possono lasciare cartelle parzialmente copiate e vengono segnalati. La copia da Explorer procede a blocchi con finestra di avanzamento, byte, tempo trascorso, stima del tempo restante e annullamento. La funzione di basso livello `dm_fs_copy` resta sincrona; non usarla per operazioni lunghe nei callback grafici.

Un `.dmlink` contiene un percorso, ad esempio `ux0:/data/`, oppure `app:notepad`. Il desktop usa `ux0:/data/desktop-mode/Desktop/`. La risoluzione ha un limite di otto passaggi. Fino a 24 elementi utente sono visualizzati sul desktop; le posizioni sono persistenti. Esplora risorse legge fino a 2048 elementi per cartella.

L'anteprima Linux usa soltanto cartelle dimostrative sotto `native/desktop/demo/`, mentre Vita usa le unità reali.

## Desktop remoto

Il pannello mostra l'indirizzo IP della console se disponibile, ma non avvia un server remoto. Un client RDP ordinario non può collegarsi a questa versione. RDP richiede un'implementazione server con negoziazione, trasmissione della grafica, input e gestione delle connessioni: [documentazione Microsoft](https://learn.microsoft.com/en-us/windows/win32/termserv/remote-desktop-protocol). Prestazioni e compatibilità sulla Vita richiedono ricerca e un port dedicato. Audio/messaggistica sono possibili come moduli futuri, ma non sono ancora app incluse.

## Finestre comuni Apri e Salva con nome

`dm_file_dialog` offre alle app lo stesso selettore usato da Notepad: unità, cartelle, collegamento al Desktop, nuova cartella, nome e filtro estensioni. Non occorre implementare un proprio Explorer.

```c
static void chosen(const char *path, int replace, void *context) {
    const char *text = context;
    if (dm_fs_write(path, text, strlen(text), !replace) < 0)
        dm_status("Salvataggio non riuscito");
}
/* Includere <string.h>. Il testo/context deve restare valido fino alla risposta. */
DmFileDialogOptions options = {
    DM_FILE_SAVE, "Salva con nome",
    "ux0:/data/desktop-mode/Desktop/", "Documento.txt", ".txt"
};
dm_file_dialog(&options, chosen, text);
```

Il risultato è asincrono: la callback riceve un percorso completo valido durante la callback; copiarlo per conservarlo. L'annullamento non chiama la callback. `replace` autorizza la sostituzione solo dopo conferma dell'utente. Il selettore sceglie il percorso: è l'app a leggere/scrivere i dati. `DM_FILE_OPEN` seleziona un file esistente. Controllare il risultato della richiesta: zero indica che non è stata aperta. `dm_confirm` fornisce una conferma asincrona condivisa.

## Pacchetto e compatibilità

`.dmapp` ha intestazione propria `DMAPP001`, piattaforma, ABI e dimensione del modulo nativo. Il loader verifica questi campi prima di caricare il codice; i vecchi moduli grezzi `.suprx`/`.so` non sono app installabili. Vita usa internamente il formato modulo Vita, senza installare plugin di sistema. Ricompilare le app v1/v2 con l'SDK v3. Il pacchetto non isola il codice: installare soltanto app fidate.

## Cestino e operazioni file

Il Desktop è una cartella reale, `ux0:/data/desktop-mode/Desktop/`. Le destinazioni scrivibili sono ux0, uma0, imc0, ud0 e ur0:/data. Le altre unità possono essere consultate se accessibili.

Elimina chiede conferma e sposta il file o la cartella nel Cestino della stessa unità, sotto `data/desktop-mode/Trash/`. Il Cestino raccoglie le unità accessibili e permette Ripristina senza sovrascrivere un file esistente. Svuota chiede conferma e cancella definitivamente con avanzamento e annullamento. Uno spostamento nel Cestino può essere immediato anche per un file grande. Annullare una copia conserva gli elementi completati e rimuove il file attualmente incompleto; annullare lo svuotamento conserva solo gli elementi non ancora eliminati.

## Input e selezione (API v3)

`dm_pointer_state(window, &state)` restituisce posizione relativa, pulsante tenuto e focus della finestra. Le app possono usare `tick` per seguire un trascinamento senza bloccare il desktop. Il focus è sospeso durante finestre modali e menu. `DM_KEY_SHIFT` combina le frecce/Home/End con estensione della selezione; sono disponibili anche `DM_KEY_SELECT_ALL`, `DM_KEY_CUT`, `DM_KEY_DELETE`, `DM_KEY_HOME/END` e `DM_KEY_SCROLL_UP/DOWN`. Notepad usa misure reali del testo e mantiene confini UTF-8 per selezione, taglio e sostituzione.

## Dipendenze native dei moduli

Il CMake indipendente accetta `DM_APP_LIBRARIES` e `DM_APP_INCLUDE_DIRS` (elenchi separati da punto e virgola). Per librerie Vita compilate con newlib, usare `DM_APP_NEWLIB=ON`: l'SDK inizializza heap privato da 16 MiB, allocatore e descrittori, e avvolge `__getreent` per non sostituire il TLS del desktop. Questo runtime serve callback sul thread della shell; non è un runtime per thread autonomi dell'app. Il modulo Browser usa curl/libxml2/OpenSSL e resta separato dall'eseguibile della shell.

## Installazione e disinstallazione

Pannello di controllo → Applicazioni installate mostra le app `.dmapp`. Installa sceglie un pacchetto e lo copia a blocchi nella cartella app; la shell lo carica a runtime. Disinstalla chiede conferma, richiede tutte le finestre dell'app chiuse, rimuove Start e associazioni e conserva i documenti. I pacchetti utente vengono eliminati; per le app incluse nel VPK, `app0:` è di sola lettura e viene salvata una rimozione persistente. Ripristina inclusa riattiva Browser, Notepad o Contatore. Il codice caricato resta residente fino al riavvio, anche dopo disinstallazione: sostituire la versione di un'app già caricata richiede un riavvio.

## Rinomina e aggiornamento dei percorsi

`dm_fs_rename(source, destination)` rinomina file o cartelle nella stessa directory scrivibile, senza sovrascrivere elementi esistenti. Ritorna zero al successo, negativo per errore. Unità, cartella dei dati del desktop e radice Desktop sono protette; gli elementi nel Cestino vanno prima ripristinati. I nomi devono rispettare la validazione di `dm_fs_join`.

`dm_fs_path_update(path, capacity, &revision)` applica le rinomine successive alla revisione conservata dall'app, inclusi i cambiamenti di nome delle cartelle antenate. Inizializzare la revisione al momento di aprire un documento e aggiornare il percorso in `tick` e prima delle operazioni di salvataggio. Sono conservati gli ultimi 32 eventi: un'app deve consumarli regolarmente. Queste funzioni sono aggiunte in fondo alla tabella API v3; i moduli v3 precedenti continuano a usare le funzioni già presenti. Ricompilare un'app con l'SDK aggiornato per utilizzare le nuove funzioni.

Le app `.dmapp` attuali contengono codice nativo per una sola piattaforma. La stessa API non rende universali i binari: vedere [architettura e possibili runtime portabili](architecture.md).

## Immagini e Task Manager (estensioni API v3)

`dm_tasks(array, capacity)` restituisce le finestre attive di Desktop Mode, con puntatore, titolo e stato minimizzato. Aggiornare l'elenco prima delle azioni; `dm_focus`, `dm_close` e il campo `minimized` gestiscono le finestre. È l'elenco del processo Desktop Mode, non dei processi del sistema operativo della console. `dm_close` rispetta il rifiuto delle app con documenti non salvati.

Le app possono usare `dm_image_load` per PNG/JPEG, `dm_image_create` e `dm_image_update` per pixel RGBA (`DM_COLOR`), `dm_image_size` e `dm_image_draw` per la visualizzazione. Il puntatore immagine è opaco, resta valido fino a `dm_image_free`, e va liberato alla chiusura della finestra. `dm_image_save_png(path, width, height, pixels, exclusive)` codifica e salva un PNG; richiedere il percorso e il consenso alla sostituzione con il selettore comune. Zero indica successo, negativo errore. Il buffer deve contenere `width * height` pixel.

Paint WASM usa una barra strumenti classica a due colonne con 16 strumenti (selezione, gomma, riempimento, contagocce, zoom, matita, pennello, aerografo, testo, linea, curva, rettangoli, poligono ed ellisse), palette a 28 colori, campionamento RGB, spessori e riempimenti, annulla, selezione e copia/taglia/incolla locale, zoom e barre di scorrimento. Apre BMP non compresso a 24/32 bit; salva BMP o PNG. La selezione libera è ancora rettangolare e gli appunti immagine restano dentro Paint.

I moduli separati `calculator.dmapp`, `taskmanager.dmapp`, `paint.dmapp` e `images.dmapp` sono esempi completi di queste API. Per compilarli autonomamente, usare il CMake dell'SDK (Calcolatrice richiede `DM_APP_LIBRARIES=m`). Queste aggiunte conservano la parte precedente della tabella API v3; le nuove app richiedono la shell aggiornata.

## Directory e operazioni asincrone (API v3)

`dm_fs_list(path, entries, capacity, offset)` enumera una directory in blocchi di `DmDirectoryEntry` (nome, dimensione, flag directory). Restituisce il numero letto, zero a fine elenco o un valore negativo per errore. Capacity va da 1 a 2048; offset conta solo elementi reali, escludendo `.` e `..`. Non garantisce ordine né uno snapshot se la directory cambia durante la lettura.

`dm_fs_mkdir(path)` crea una singola directory scrivibile; il padre deve esistere. `dm_copy_async(source, destination, callback, context)` e `dm_trash_async(source, callback, context)` avviano le operazioni comuni con avanzamento e annullamento. Ritorno zero indica avvio; negativo indica rifiuto. La callback riceve 1 al successo, 0 per annullamento, -1 per errore. La copia non sovrascrive un nome esistente. La richiesta di consenso per eliminare è responsabilità dell'app prima di chiamare `dm_trash_async`. Il context deve restare valido fino alla callback: Console impedisce la chiusura mentre l'operazione è attiva.

`console.dmapp` è un'app C separata che usa questi servizi. Accetta comandi built-in con alias Windows/Linux, percorsi Vita assoluti o relativi, separatori `/` e `\` e nomi tra virgolette. Implementa help, dir/ls, cd/chdir, pwd/cwd, copy/cp, del/delete/rm/rmdir/rd, ren/rename/mv, mkdir/md, type/cat, touch, echo, cls/clear, history, units/drives, ver/uname, open, start ed exit. `mv` in questa versione rinomina nella stessa cartella; non sposta tra directory o unità. Eliminare usa il Cestino con conferma, anche per le cartelle. Non implementa opzioni GNU/CMD, wildcard, redirezioni, pipe, script, eseguibili Windows/Linux o shell del sistema. `touch` crea un file nuovo e conserva un file già esistente. `type/cat` leggono testo fino a 32767 byte; il registro conserva le ultime 192 righe. `dir/ls` mostra fino a 1024 elementi.

Il modulo PDF nativo è incluso in Linux e Vita, usa le API immagini/file disponibili e collega la libreria C del progetto `libdesktop_pdf.a`. Supporto e limiti sono descritti nello [stato PDF](pdf-viewer.md). `DM_PLUGIN_HEAP_SIZE` configura l'heap newlib di un modulo che attiva `DM_PLUGIN_NEWLIB`; il default resta 16 MiB.


## Esegui e desktop remoto

Start contiene una voce fissa **Esegui...**, separata dalle pagine delle app. Accetta percorsi di cartelle, file con associazioni, collegamenti `.dmlink`, pacchetti `.dmapp`, ID e titoli delle app installate. Un pacchetto `.dmapp` valido viene caricato e avviato; un documento viene passato all'app associata. Non esegue binari Windows `.exe` né comandi di shell: per `dir`, `ls`, `copy` e gli altri comandi usare Console. I percorsi relativi partono dalla cartella Explorer in primo piano oppure dal Desktop; i nomi con spazi possono essere racchiusi tra virgolette.

Il server RDP della shell si configura dal Pannello di controllo → Desktop remoto RDP; vedere [protocollo, collegamento e limiti verificati](remote-desktop.md). Non richiede una modifica dei moduli `.dmapp`: gli eventi remoti usano gli stessi callback `click`, `text` e `key` della shell.
