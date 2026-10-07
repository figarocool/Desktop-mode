# Stato delle richieste: rete, Bluetooth, unità USB

## Aggiunte del 5 ottobre 2026

La taskbar include un'icona di rete separata da Bluetooth e volume. Mostra le barre Wi-Fi oppure il simbolo LAN quando il backend riconosce Ethernet; stato non disponibile e disconnessione hanno rappresentazioni distinte. Il clic apre **Connessioni di rete**, disponibile anche in Start e Pannello di controllo. I quattro posti per le app nella tray rimangono disponibili.

Il pannello mostra la connessione attuale e i valori disponibili: nome/profilo, SSID, MAC, IP, subnet, gateway, DNS, MTU e segnale o proxy. Sulla Vita li legge dalle API native SceNetCtl; su Linux cerca l'interfaccia attiva e legge i dati disponibili da getifaddrs, sysfs, route e resolver. Lo stato Linux non viene presentato come stato della console. Per Wi-Fi/Ethernet il campo device usa la convenzione 0=wireless, 1=wired; il riconoscimento e le letture PSTV richiedono collaudo hardware.

Il pannello ora legge lo stato della radio Wi-Fi e consente di accenderla o spegnerla dalla nostra finestra, tramite le esportazioni userland `sceWlanGetConfiguration`/`sceWlanSetConfiguration` verificate nell'app di sistema. Bluetooth ha gli stessi controlli diretti tramite `sceBtGetConfiguration`/`sceBtSetConfiguration`; ricerca e associazione restano nel pannello Bluetooth.

Il pannello avvia la ricerca degli access point tramite esportazioni userland private `SceNetCtl` ricostruite dall'app Impostazioni, mostra SSID e dati del record e invia SSID/password alla transazione di profilo ricostruita. La lista gestisce fino a 16 reti, mostrandone due per pagina. La scansione e il submit del profilo non sono ancora collaudati su Vita/PSTV: le firme non sono documentate e il firmware può richiedere ulteriori campi specifici per tipo di sicurezza. DHCP/IP statico/DNS non sono ancora configurabili dal pannello. I pulsanti non aprono le Impostazioni ufficiali. Su Linux la radio è solo in anteprima e la scansione non è disponibile. Le chiamate radio Wi-Fi e Bluetooth richiedono collaudo hardware Vita/PSTV.

Esplora risorse controlla ogni due secondi le unità note montate dal sistema. Se compaiono/scompaiono, aggiorna l'elenco Computer e il selettore Apri/Salva, differendo il ridisegno durante le operazioni/modalità attive. Il codice **non monta da solo periferiche USB**, non assegna automaticamente `uma0:` e non cambia il ruolo di `ux0:`. Le unità già montate e accessibili si navigano con le operazioni locali dell'ambiente.

Il pannello SMB consente scansione IPv4 della sottorete, accesso alle condivisioni, navigazione, download con barra di avanzamento, upload a blocchi con barra di avanzamento, rinomina, creazione cartelle ed eliminazione remota con conferma. L'upload crea un temporaneo remoto senza sovrascrivere nomi esistenti e lo rinomina a trasferimento concluso. Se la connessione cade durante l'invio, il temporaneo può restare sul server; il pannello lo segnala. Sono supportati file singoli, non il trasferimento ricorsivo di intere cartelle. L'eliminazione delle cartelle richiede che siano vuote. Le credenziali vivono solo in memoria e vengono rimosse alla chiusura o con Stop. Il rilevamento prova i servizi IPv4 comuni e non identifica necessariamente tutti i dispositivi.

La compilazione VPK verifica l'integrazione del backend; non sostituisce il collaudo di credenziali, permessi e trasferimenti su una rete reale e su hardware Vita/PSTV.

## Bluetooth

Il backend C usa lo stack Bluetooth della Vita, importando le funzioni userland SceBt per inquiry, callback, connessione, disconnessione e risposte PIN/conferma. SceHid alimenta l'input di tastiera e mouse. Sono presenti pannello, icona tray e controlli per i dispositivi scoperti. La macchina di stati è stata verificata con eventi simulati e sanitizzatori; il VPK usa gli stub nativi.

Resta da verificare associazione e rimozione su Vita/PSTV, soprattutto l'interpretazione dell'evento PIN, i permessi del firmware e i modelli delle periferiche. La radio deve essere attiva nelle impostazioni della console. La lista e la rimozione usano le esportazioni userland SceBt, senza chiamate kernel. Non sono gestiti tutti i profili Bluetooth/audio. Vedere [dettagli e ABI](selection-and-devices.md).

## USB PSTV: memoria e lettori ottici

VitaShell documenta il montaggio di memorie USB FAT32/exFAT su PSTV come `uma0:`. È una prova di supporto alla memoria di massa, non una prova che qualsiasi lettore CD/DVD sia compatibile. Lettori di schede o dischi USB devono essere riconosciuti dal driver e avere un filesystem supportato; quando una partizione viene montata, l'ambiente può mostrarla come unità.

Per un lettore CD/DVD sono ancora da sviluppare e verificare il trasporto USB/compatibilità hardware, i comandi SCSI/MMC del lettore, il filesystem ISO9660/UDF dei dischi dati e, per CD audio, lettura della TOC e dei settori CD-DA. Un CD audio non presenta file MP3/WAV normali. Servono inoltre un'app player separata, il backend audio e un servizio di inserimento/AutoPlay. Le API SceUsbd di enumerazione e trasferimento offrono una base da studiare, senza garantire compatibilità con un particolare lettore. Nessun lettore ottico è attualmente supportato o collaudato dal progetto.

## Altre richieste ancora incomplete

- Lettore musicale separato, messaggistica ed editor di documenti formattati tipo Word.
- Browser completo: il renderer interno attuale è per testo/link; mancano CSS, immagini, moduli e script dei siti. Schede e preferiti sono presenti.
- Compatibilità binaria universale PSP/Vita/PS3: esiste l'API comune, ma mancano backend PSP/PS3 e un runtime comune per eseguire la stessa app senza ricompilarla. I moduli attuali sono compilati per piattaforma.
- Screensaver Windows `.scr`/binari Linux: attualmente sono accettati solo moduli nativi `.dmsaver` compatibili con la piattaforma.
- La shell, i dialoghi comuni, le app incluse e i messaggi di sistema hanno il catalogo IT/EN/ES. Le app aggiunte da terzi devono fornire le proprie stringhe con `dm_localize`; il testo utente, i percorsi e i contenuti dei documenti sono mostrati invariati tramite `dm_text_raw`.
- SMB: disponibili scansione, navigazione, download/upload di file singoli, rinomina, creazione cartelle e cancellazione remota; mancano trasferimento ricorsivo, autenticazione salvata e collaudo hardware/rete reale.
- PDF: visualizzatore funzionante con compatibilità parziale dei documenti, non tutti i costrutti PDF.
- RDP: server TLS/password e desktop/input reali sono presenti, ma mancano NLA, canali audio/appunti e altre funzioni; resta il collaudo sulla rete Vita e con il client Windows.
- Clock CPU/GPU: selezione tramite API ufficiali e reset presenti; overclock oltre i profili disponibili richiede ulteriore supporto specifico.

## Fonti primarie consultate

- [API SceNetCtl VitaSDK](https://docs.vitasdk.org/group__SceNetCtlUser.html)
- [Guida ufficiale delle connessioni PSTV](https://manuals.playstation.net/document/en/pstv/settings/internet.html)
- [URI delle impostazioni documentati da vita-uriCaller](https://github.com/Freakler/vita-uriCaller)
- [VitaShell: USB FAT32/exFAT e montaggio uma0](https://github.com/TheOfficialFloW/VitaShell#how-to-use-an-usb-flash-drive-as-memory-card-on-a-ps-tv)
- [API USB host SceUsbd VitaSDK](https://docs.vitasdk.org/group__SceUsbdUser.html)
