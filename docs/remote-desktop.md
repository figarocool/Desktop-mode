# Desktop remoto RDP

Il server è scritto in C e integrato nella shell. Un client condivide la stessa immagine e le stesse finestre visibili sulla Vita e le controlla con mouse e tastiera; non viene avviata una seconda sessione o un altro homebrew.

## Abilitazione

1. Aprire **Pannello di controllo → Desktop remoto RDP**, oppure l'app omonima da Start.
2. Scegliere una password ASCII da 8 a 127 caratteri. L'utente è **desktop**.
3. Lasciare la porta **3389**, oppure cambiarla prima di abilitare il server.
4. Premere **Abilita server**. Il pannello mostra l'IP della Vita e la porta effettiva.
5. Collegarsi dalla stessa rete con un client RDP configurato per **TLS senza NLA/CredSSP**.

Il server è disabilitato all'avvio. La password resta in memoria e non è salvata nelle preferenze; va impostata nuovamente quando si riavvia l'app. Chiudere la finestra di configurazione lascia il servizio attivo; **Disabilita server** chiude il client e libera i buffer. Per cambiare password o porta, disabilitare il server e riabilitarlo.

Il certificato TLS autofirmato e la chiave vengono generati localmente e salvati in `ux0:/data/desktop-mode/rdp-cert.pem` e `rdp-key.pem`. Il client può chiedere di accettare il certificato. Il server non effettua configurazioni del router o port forwarding; questa prima versione è destinata alla rete locale.

## Client

Con FreeRDP:

```sh
xfreerdp /v:IP_DELLA_VITA:3389 /u:desktop /sec:tls /size:960x544 /bpp:16
```

Alcune installazioni chiamano il programma `xfreerdp3`. Inserire la password nella richiesta del client. Lasciare disattivati clipboard, audio e redirezione di unità: non sono implementati.

Per il client Windows è incluso [desktop-mode.rdp](desktop-mode.rdp). Modificare `full address` con IP e porta della Vita, aprire il file e inserire utente/password. Il profilo richiede TLS con CredSSP disabilitato. La compatibilità con `mstsc` **non è stata provata**; un client che impone NLA non è supportato da questa implementazione.

## Funzioni e limiti

- Una connessione alla volta, autenticata dentro TLS; nessuna connessione RDP in chiaro.
- Desktop fisso **960×544**, RGB565 a 16 bit, aggiornamenti bitmap non compressi di 64×32 pixel. Si inviano i blocchi modificati; gli aggiornamenti sono limitati per mantenere reattiva la shell. Non è un trasporto video ad alta frequenza.
- Clic sinistro, tasto destro, trascinamento, rotella e Ctrl/Shift per selezionare. I modificatori sono conservati per ogni evento, anche se premuti e rilasciati nello stesso gruppo di pacchetti.
- Tastiera: scancode con mappatura base US, frecce, Home/End, Delete, Backspace, Enter e Ctrl+A/C/X/V/S; testo Unicode BMP, incluso il testo accentato. Non implementati layout scancode nazionali completi, coppie surrogate, Alt/AltGr e scorciatoie dei tasti Windows.
- Nessun NLA/CredSSP, clipboard tra PC e Vita, audio remoto, redirezione file/stampanti, multi-monitor o ridimensionamento remoto.
- Buffer e dimensioni dei pacchetti limitati; timeout di negoziazione di 30 secondi e pausa dopo credenziali errate. Il pannello mostra disconnessioni ed errori di ascolto.
- Circa 2 MiB per l'immagine, oltre a buffer di rete e memoria OpenSSL. VitaSDK presente usa OpenSSL 1.0.2 e TLS 1.2; l'anteprima Linux usa OpenSSL del sistema. L'allocatore e il costo sulla console richiedono verifica hardware.

## Verifiche eseguite

La build Vita comprende server, pannello, capture tramite `sceDisplayGetFrameBuf`, input remoto e libreria TLS. **Nessuna prova su Vita reale è stata effettuata.**

Il test `python3 scripts/test-rdp.py` usa un client **FreeRDP 3.32.0 reale** con una libreria del server compilata dagli stessi sorgenti. Il trasporto è in memoria, con handshake **TLS 1.2 effettivo**, senza socket e senza scrivere dati dell'utente. Verifica negoziazione RDP, password e utente errati, richiesta iniziale 16/32 bit, decodifica bitmap e orientamento/colore ai quattro angoli, mouse, trascinamento, tasto destro, Unicode, Enter e Ctrl-clic. Non verifica la cattura del framebuffer Vita, il Wi-Fi o il client Windows.

Il parser ha inoltre superato 10.000 pacchetti malformati con ASan/UBSan, test di troncamento, doppio join di un canale, autenticazione, limiti del mouse e input. La suite desktop verifica anche Esegui, associazioni, file con spazi e cartelle relative. Questi controlli non costituiscono una revisione di sicurezza completa del protocollo.

In questo ambiente gli socket, anche locali, restituiscono `Operation not permitted`: la connessione TCP completa rimane da verificare in un ambiente che consenta la rete. Per l'anteprima su un PC normale, avviare `scripts/desktop-preview.sh`, abilitare il server dal pannello e usare `127.0.0.1:3389`; da un altro PC utilizzare l'IP LAN del computer.

## Riferimenti

Strutture e sequenza implementate secondo [Microsoft MS-RDPBCGR](https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpbcgr/023f1e69-cfe8-4ee6-9ee0-7e759fb4e4ee), T.124/GCC e T.125/MCS. Interoperabilità verificata tramite [API di trasporto FreeRDP](https://github.com/FreeRDP/FreeRDP/blob/3.32.0/include/freerdp/transport_io.h) e decodificatore bitmap/GDI. Il server distribuito non dipende da FreeRDP o WinPR.
