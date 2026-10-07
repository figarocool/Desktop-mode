# Architettura attuale: Desktop Mode App in C

Desktop Mode è un unico processo homebrew. La shell nativa gestisce desktop, mouse, finestre, taskbar, menu Start e servizi comuni. Le app aperte hanno finestre e stato propri nello stesso processo; non avviano un altro EBOOT. Minimizzare conserva lo stato, chiudere libera la finestra. I moduli caricati restano residenti fino al riavvio.

I backend traducono input, grafica, filesystem, batteria e rete in servizi della piattaforma: VitaSDK/vita2d su Vita, SDL2 e adattatori locali nell'anteprima Linux. PSP e PS3 non hanno ancora un backend implementato.

Le app esterne sono moduli C contenuti nei pacchetti `.dmapp`. Il loader verifica intestazione, piattaforma e versione API, carica il modulo e ne registra descrittore, callback e associazioni delle estensioni. Notepad, Browser e Contatore usano questo sistema; aggiungere un'app non richiede di modificare o ricompilare la shell. Le app chiamano una tabella di funzioni dell'SDK per disegnare, ricevere input, leggere/scrivere file e aprire i selettori comuni. Non sono isolate: il codice nativo deve essere fidato.

## Portabilità e app universali

L'API comune rende riutilizzabile il sorgente delle app che usano soltanto i suoi servizi, ma il binario nativo dipende dalla CPU e dal formato dei moduli. Vita usa ARM, PSP MIPS e PS3 PowerPC. Gli attuali pacchetti contengono un solo modulo per una piattaforma: richiedono compilazioni distinte.

Un pacchetto con più binari può semplificare la distribuzione, ma richiede comunque di compilare ogni variante. Per compilare l'app una sola volta serve un formato eseguibile indipendente dalla CPU e un interprete/macchina virtuale portato su ogni console. Una possibile evoluzione è un nuovo tipo di pacchetto con bytecode WebAssembly prodotto da sorgenti C e un SDK di funzioni importate dalla shell. Non richiede JavaScript o HTML.

Questo runtime non è implementato. Occorre verificare interprete, memoria, gestione dei moduli e librerie su ogni console, in particolare PSP. Non basta inserire l'attuale modulo ARM in un contenitore universale. Le app che chiamano direttamente VitaSDK o dipendenze native come quelle del Browser devono essere adattate al nuovo SDK.

## Percorsi e rinomina

Il filesystem espone unità Vita reali; Linux le simula con cartelle demo. La rinomina locale aggiorna i percorsi delle finestre Explorer, gli appunti dei file e i percorsi dei moduli registrati. Le app possono seguire le rinomine attraverso `dm_fs_path_update`: Notepad lo usa anche prima di salvare. Il selettore Apri/Salva offre Rinomina ai programmi che lo richiamano. La vista Rete usa separatamente la rinomina SMB asincrona.

Per il contratto delle app, vedere [API e SDK](desktop-api.md). I documenti di ricerca sul runtime PocketJS riguardano una proposta precedente e non descrivono l'implementazione attuale.
