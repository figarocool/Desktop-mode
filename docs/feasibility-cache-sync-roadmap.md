# Play OS: cache, app universali, media, remoto e sincronizzazione

Valutazione preliminare del 2 ottobre 2026. Integra [analisi delle app e dei runtime](runtime-and-apps-analysis.md). È un piano di fattibilità, non una dichiarazione che le funzioni siano già implementate. Il progetto locale contiene prototipi di shell/SDK e test di modello; non ci sono build console validate.

## Requisito centrale

Ogni app viene pubblicata una volta come pacchetto portabile e gira dentro Play OS. La versione del runtime cambia per macchina. Le app non includono binari nativi specifici e non avviano un altro homebrew. I backend possono scegliere decoder, renderer, buffer e modalità di input diversi, conservando API e formati dati comuni.

Il pacchetto unico non dimostra che qualsiasi app web esistente funzioni su ogni dispositivo. Occorre definire e provare il contratto minimo; il riuso senza port delle app web richiede API browser compatibili. La cache non sostituisce tali API.

## Servizio cache comune

Il runtime espone un servizio interno condiviso con budget per macchina e quote per app. RAM: risorse di uso immediato, buffer, layout e oggetti attivi. Storage: pacchetti, miniature, risorse ricostruibili e copie temporanee dei contenuti remoti. Prefetch limitato e lettura a blocchi. Espulsione delle risorse meno usate o meno costose da ricaricare; nessuna espulsione di file utente o download marcati offline.

Le chiavi devono includere identità/versione della risorsa e parametri di trasformazione, per non mostrare risultati vecchi. La cache può essere ricostruita. Le scritture devono essere aggregate e completate senza lasciare un file valido a metà. Un limite globale evita che ogni app riservi buffer indipendenti troppo grandi. Sul PC si possono mantenere più dati caldi, sulle console buffer contenuti e risorse ricaricabili.

La cache riduce letture ripetute, rete e ricomputazione; non rende più veloce ogni accesso. I cache miss e le scritture hanno un costo. Va misurata con memoria totale del runtime, latenza di apertura e interazione, audio underrun e volume delle letture/scritture. Lo swap generico degli oggetti JS non è incluso nella prima versione: richiederebbe un meccanismo diverso dalla cache delle risorse.

## Trasferimento e sincronizzazione

Il PC può ospitare Play OS e un servizio companion di scambio dati. La console mantiene una replica locale. Il collegamento può iniziare sulla rete locale; le possibilità USB dipendono dal target e vanno implementate/verificate separatamente. Non si presume l'esistenza di un client Syncthing ufficiale per queste console.

Un protocollo dedicato e contenuto può usare manifest, revisioni, hash e trasferimenti riprendibili a blocchi. Scrivere file temporanei, verificare l'integrità e pubblicare il file completato. Nel caso di due modifiche offline dello stesso documento conservare entrambe le copie o usare un merge specifico del formato; non scegliere sulla sola base dell'ora. Le cancellazioni hanno revisioni esplicite, i nomi devono essere compatibili tra filesystem e il disco pieno non deve distruggere la copia valida.

L'abbinamento dei dispositivi e l'accesso autenticato sono parte del servizio. Iniziare con LAN; accesso Internet, gestione connessioni e discovery remoto sono lavoro aggiuntivo.

Sincronizzare: pacchetti app compatibili, preferenze, documenti, salvataggi e media selezionati. Le cache di rendering possono essere locali e ricostruite; non occorre copiare quelle del PC sulla PSP. Metadati e catalogo possono essere condivisi anche se i contenuti grandi restano sul PC.

Il comando 'Disponibile offline' deve conservare app, file e dipendenze necessarie. Si può andare via solo dopo che le copie selezionate sono complete. Il PC spento non impedisce l'uso delle copie locali, ma interrompe accesso ai contenuti rimasti remoti e ai servizi che esegue. Lo stato applicativo deve avere un formato serializzabile versionato; trasferire tutti gli oggetti vivi di una VM non è previsto.

Un sistema di sincronizzazione non equivale a un backup: mantenere versioni/copie recuperabili separatamente. Come riferimento per blocchi, file temporanei e conflitti: [documentazione Syncthing](https://docs.syncthing.net/users/syncing.html). I dettagli del protocollo devono essere adattati al budget delle console.

## Esempi di flusso

MP3: trascino sul PC → trasferimento o sincronizzazione → salvataggio locale sulla console → selezione nel player Play OS → decoding progressivo e output audio. La [PSPSDK](https://pspdev.github.io/pspsdk/pspmp3_8h.html) espone le funzioni MP3; [VitaSDK](https://docs.vitasdk.org/group__SceAudiodecUser.html) espone decoder audio. Queste API rendono l'obiettivo concreto, ma richiedono adapter, buffering e prove reali. Il player deve rimanere dentro Play OS.

Video: conservare originale e, se necessaria, una rendition compatibile generata dal PC. Identità del contenuto comune, versioni multimediali selezionate dal backend; il pacchetto dell'app resta identico. Un'estensione MP4 non specifica da sola codec, profilo e bitrate. Le specifiche delle app ufficiali descrivono capacità della piattaforma, non garantiscono il funzionamento nel nostro homebrew. [PSP video](https://manuals.playstation.net/document/en/psp/current/video/filetypes.html), [Vita video](https://manuals.playstation.net/document/en/psvita/videos/filetypes.html).

Documento: trasferire il file conserva i byte. Aprirlo/modificarlo richiede un'app che implementi il formato. TXT e piccoli documenti in un formato Play OS sono obiettivi iniziali. DOCX/XLSX con fedeltà completa richiedono import/export e layout avanzati; una preview convertita dal PC non equivale all'editing completo offline.

## Tre funzioni remote

1. Telecomando/gestione: dal PC inviare file, aprire un documento, controllare playback e consultare stato della console. È il primo obiettivo remoto.
2. Desktop della console sul PC: trasmettere schermo e input. Occorrono cattura, trasporto, codec, routing degli eventi e misure di latenza. Sulla PSP può limitarsi a un aggiornamento più lento per interfacce statiche, da misurare.
3. Desktop PC sulla console: il PC esegue app e la console riceve immagini/audio e manda input. Permette app pesanti col PC raggiungibile, ma non soddisfa il requisito dell'esecuzione locale offline di quelle app.

Il servizio di sync è indipendente dai tre. Un desktop remoto non deve essere obbligatorio per trasferire file o sincronizzare.

## Catalogo realistico per una prima versione

Nessuna app è garantita oggi al 100% su tutte le piattaforme. Le garanzie si riferiranno a build e modelli verificati, casi di test, dimensioni dei dati, formati e API supportate.

| App | Ambito iniziale proposto | Valutazione |
|---|---|---|
| File manager | Cartelle, copia, spostamento, rename, apertura tramite associazioni | Priorità alta, scope concreto |
| Calcolatrice | Aritmetica e memoria; scientifica dopo prova libreria | Priorità alta |
| Blocco note | TXT, input console, apertura/salvataggio | Priorità alta |
| Orologio e timer | Ora locale, timer; lavoro sospeso da gestire | Priorità alta |
| Impostazioni | Tema, input, storage, dispositivi associati | Priorità alta |
| Solitario | Regole, sprite, input tastiera/controller e salvataggio | Priorità alta, port iniziale |
| Campo minato | Griglia e input controller | Priorità alta, port iniziale |
| Tetris/Forza quattro | Giochi 2D piccoli | Priorità alta, port iniziale |
| Galleria | JPEG/PNG, miniature, immagini con limiti di dimensione | Priorità alta, decoder da integrare |
| Player musicale | MP3/WAV selezionati, playlist, seek e background | Concreto, adapter media da validare |
| Paint semplice | Tela limitata, pennello, gomme e salvataggio | Concreto; JS Paint completo più ampio |
| Gestore sync/store | Trasferimenti, stato offline, installazione pacchetti | Concreto; store pubblico fase distinta |
| Video player | Codec/profili validati, conversione PC facoltativa | Condizionato ai decoder e alle prove |
| Editor formattato | Piccoli documenti e formato definito | Condizionato al runtime di editing |
| Foglio di calcolo | Griglia piccola, CSV, formule selezionate | Condizionato al runtime e al budget |
| Webamp completo | Vista, skin, equalizzatore, visualizzatori | Port importante; non garanzia PSP |
| Office completo/PDF complessi | Fedeltà avanzata e formati grandi | Nessuna promessa universale iniziale |

## Stima di lavoro

Stima d'ordine di grandezza per una persona esperta a tempo pieno, con dispositivi, SDK e un perimetro limitato. Non è una scadenza né una somma di attività già pianificate; frontend, runtime e sync condividono lavoro. L'assenza di build hardware validate rende la confidenza bassa. I tempi possono cambiare oltre il doppio dopo le prove.

| Blocco | Intervallo preliminare | Condizioni |
|---|---|---|
| Prova pacchetto/JS/rendering/media su PC e console | 2–4 settimane | Toolchain disponibili; scegliere una console iniziale |
| Core Play OS su PC: loader, finestre, file, dati, quote e cache | 1–3 mesi | Profilo applicativo definito |
| Adapter e validazione Vita | 1–3 mesi | Riuso di un host nativo funzionante; niente browser completo nuovo |
| Adapter e validazione PSP | 2–6 mesi | UI/API limitate; portabilità del codice provata |
| Trasferimento e sincronizzazione bidirezionale LAN | 1–3 mesi | Protocollo limitato, conflitti conservati e prove di interruzione |
| Player audio/video e conversione companion | 1–3 mesi | Formati/profili scelti; non tutto il catalogo codec |
| Controllo remoto e visualizzazione desktop | 1–3 mesi | Prima desktop statico; latenza gaming non garantita |
| Store pubblico iniziale | 1–3 mesi | Upload, catalogo, compatibilità e pacchetti; hosting disponibile |
| Port selezionati di app web complesse | 1–6+ mesi per app | Può scendere con runtime web compatibile; nessuna stima unica per tutto Office |
| PS3/PS4 | Da stimare dopo prova SDK/backend | Nessun port funzionante validato qui; ipotesi di alcuni mesi per target |

Come orizzonte complessivo condizionato: circa 6–12 mesi per una prima versione locale PC/Vita/PSP con poche app semplici, cache, trasferimento e sync di base, se si evita costruire un browser completo. Circa 12–24+ mesi per una versione più matura con più app, media, remoto e store; l'inclusione PS3/PS4 può allungarlo e non è coperta da una data affidabile. Una compatibilità browser estesa su tutte le console è un progetto di ricerca/port più ampio, senza stima affidabile prima del prototipo.

Non assumere che rallentare la PSP renda automaticamente possibile qualsiasi app PC. Supporto API, memoria e formato devono passare separatamente. Il requisito del pacchetto unico si verifica usando gli stessi byte e SHA-256, non solo il medesimo nome dell'app.

## Primo traguardo verificabile

Stesso pacchetto di calcolatrice/gioco e player di prova su PC e una console; invio di MP3/TXT; playback/input dentro Play OS; sincronizzazione interrotta e ripresa; uscita offline con PC spento; riconnessione con recupero delle modifiche e conservazione dei conflitti. Misurare memoria totale, risposta, durata operazioni e stabilità. Prima di promettere PSP/Vita/PS3/PS4 complete, ciascun backend deve superare questo traguardo.
