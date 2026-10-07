# Selezione e dispositivi

## Selezione multipla

Desktop: fino a 32 icone (8 di sistema e 24 file). Explorer: fino a 2048 elementi per directory. Ctrl+clic aggiunge/toglie, Shift+clic seleziona un intervallo nell'ordine dell'elenco, Ctrl+A seleziona tutto. Sulla console SELECT+X aggiunge/toglie. Il rettangolo parte dal fondo vuoto del desktop o da uno spazio vuoto/margine destro della lista Explorer. La selezione evidenzia tutti gli elementi e il clic destro su un elemento gia selezionato conserva il gruppo.

Le posizioni delle icone vengono spostate insieme e conservate; la selezione del desktop segue i nomi durante l'aggiornamento. Copia cattura una lista indipendente di percorsi. Incolla e spostamento nel Cestino usano `multi_file.c`, che esegue una coda delle operazioni incrementali comuni, si ferma al primo errore/annullamento e non sovrascrive nomi esistenti. Ogni elemento mostra il proprio avanzamento. Gli elementi gia completati rimangono tali. Una conferma copre l'intero gruppo da eliminare. Le icone di sistema non sono file da eliminare.

Rinomina resta un'operazione su un solo elemento. Il selettore comune Apri/Salva conserva il contratto di un singolo percorso; non e un dialogo di apertura multipla.

## Volume

`device_ui.c` fornisce icona speaker, slider e disattivazione/riattivazione. SceAVConfig usa livelli 0-30; il valore viene letto dal sistema, evitando di inventare uno stato locale sulla console. Se l'API fallisce viene mostrato l'errore. Nell'anteprima il volume e simulato e non controlla l'audio Linux.

## Bluetooth e HID

L'app integrata Bluetooth e dispositivi e raggiungibile da taskbar, Start e pannello di controllo. `bluetooth_native.c` richiama le esportazioni **userland SceBt** presenti in `VitaSDK/share/vita-headers/db/360/SceBt.yml`, tramite `SceBt_stub`: inquiry, lettura eventi, nomi, connessione/disconnessione, PIN e conferma utente. Il solo header kernel non dimostrava l'assenza di API userland; quella conclusione precedente era errata. Non installa plugin di sistema.

Il pannello mostra fino a 32 dispositivi scoperti e associati, permette accensione/spegnimento radio, ricerca, associazione, disconnessione, aggiornamento e **Dimentica** con conferma. Usa le esportazioni userland SceBt (`sceBtSetConfiguration` per la radio, oltre alle API di inquiry, callback, connessione, PIN e conferma); non importa le equivalenti `ksce*` kernel. Ferma l'inquiry prima della connessione e attende l'evento di arresto. Richiede PIN/conferma soltanto per il dispositivo scelto; non conferma automaticamente associazioni. Alla chiusura arresta la ricerca e rimuove il callback, senza disconnettere i dispositivi già utilizzabili. La lettura usa gli slot registrati e la struttura SceBtRegisteredInfo da 0x100 byte. Permessi firmware, enumerazione effettiva, formato completo degli eventi (in particolare PIN id 3), accensione/spegnimento, associazione/rimozione e compatibilità delle periferiche richiedono collaudo hardware. Linux non simula dispositivi nelle vicinanze: la scansione restituisce non disponibile.

`input_devices.c` usa SceHid per mouse, pulsanti, wheel, tastiera italiana UTF-8, AltGr, Ctrl/Shift, Caps Lock e tastierino numerico. Ripetizione dopo 450 ms, poi ogni 40 ms; Ctrl scorciatoie non ripetute. Report contigui con cast richiesto dall'ABI, timestamp elaborati una volta, stati azzerati alla disconnessione. La mappa italiana e attualmente fissa; la tastiera Linux usa il layout configurato nel sistema operativo.

Verificati self-test selezione/volume, report HID simulati e macchina di stati Bluetooth simulata (callback, errore radio, inquiry, stop prima di connect, PIN/conferma, disconnect e cleanup) con ASan/UBSan. Compilato VPK con SceBt_stub, SceKernelThreadMgr_stub, SceAVConfig_stub e SceHid_stub. Non sono test di associazione su hardware.

Fonti: https://github.com/vitasdk/vita-headers/blob/master/db/360/SceBt.yml ; https://docs.vitasdk.org/group__SceBtKernel.html ; https://git.shotatoshounenwachigau.moe/vita/vds-libraries/commit/?id=587881517e9530e1356a91fd763cfcdfa0018603 ; https://github.com/xerpi/ds4vita/blob/master/main.c ; https://docs.vitasdk.org/group__SceHidUser.html .
