# Visualizzatore PDF nativo

`pdf.dmapp` è un'app C separata, associata a `.pdf`, ora inclusa nel VPK Vita. Apri usa il selettore condiviso; sono disponibili pagina precedente/successiva, Vai alla pagina, adattamento alla finestra, zoom 100–400%, scorrimento e ridimensionamento. Input fino a 16 MiB. Le allocazioni interne del motore hanno un tetto di 40 MiB; la rasterizzazione avviene sul thread dell'app e documenti complessi possono rallentare temporaneamente l'ambiente.

## Libreria del progetto

`native/pdf/desktop_pdf.c` implementa parser e rasterizzatore software; l'interfaccia pubblica è `native/apps/pdf_engine.h`. La build CMake produce `libdesktop_pdf.a`, collegata al modulo PDF. Non occorre scaricare MuPDF. Le dipendenze già disponibili in VitaSDK sono zlib, FreeType, libjpeg e le loro dipendenze statiche. Il font di riserva DejaVu Sans è incluso in `app0:/assets/pdf-font.ttf`; licenze e attribuzioni sono in `native/licenses/`.

Build indipendente della libreria:

```sh
./scripts/build-pdf-library.sh linux
./scripts/build-pdf-library.sh vita
```

Produce `native/build/pdf-library-PLATFORM/libdesktop_pdf.a` e l'header pubblico. Il chiamante conserva i byte di input fino a `pdf_engine_close`; passa un buffer RGBA al renderer e controlla sempre il codice di errore. Per distribuirla servono anche il font e le librerie indicate. Su Linux il font è letto da `native/assets/pdf-font.ttf`, sulla Vita da `app0:/assets/pdf-font.ttf`.

## Supporto implementato e limiti

Oggetti indiretti, dizionari, array, stringhe letterali/hex, object stream Flate, albero pagine e MediaBox/CropBox ereditati; contenuti singoli o multipli; trasformazioni, tracciati, curve Bézier, riempimenti nonzero/even-odd, contorni, colori Gray/RGB/CMYK, clipping rettangolare e trasparenza semplice. Testo con posizionamento, spaziatura e array TJ; font TrueType/CFF incorporati tramite FreeType, ToUnicode bfchar/bfrange, font TrueType CIDFontType2 Identity-H e CIDToGIDMap. Immagini Gray/RGB a 8 bit, Flate con predictor TIFF/PNG e JPEG, XObject Form senza gruppi di trasparenza.

Questo motore **non è un'implementazione completa di ISO 32000**. Non supporta cifratura/password, font Type3, Type1 incorporati e CIDFontType0, CMap CID diversi da Identity-H, testo/pagine ruotati, clipping non rettangolare, maschere immagini, gruppi/fusioni di trasparenza avanzati, gradienti/pattern, filtri concatenati e ulteriori codec. Queste caratteristiche restituiscono un errore esplicito. Non visualizza annotazioni, moduli o livelli interattivi; non esegue script, programmi, allegati o collegamenti. I font standard non incorporati sono sostituiti da DejaVu Sans, con possibili differenze di aspetto e spaziatura. Rasterizzazione software senza antialias dei tracciati e con contorni approssimati nelle giunzioni/estremità. Le funzioni non supportate non vanno descritte come compatibili.

## Anteprima e alternative

L'anteprima usa per default lo stesso motore nativo del VPK. `DESKTOP_PDF_NATIVE=0 ./scripts/desktop-preview.sh` abilita il precedente backend Poppler/Cairo Linux, che offre compatibilità più ampia e password. Il backend MuPDF rimane opzionale con `DM_PDF_MUPDF_ROOT`, per librerie effettivamente compilate per Vita; quel ramo resta non verificato.

## Verifiche

Vita: compilati libreria, modulo `.dmapp` e VPK; prova su hardware ancora necessaria per loader, memoria e rendering. Linux: modulo nativo integrato, associazione, due pagine con pixel rosso/blu, zoom/scorrimento/ridimensionamento. PDF reale generato da Cairo con testo accentato/font incorporato e compressione Flate; PDF 1.5 con object/xref stream, immagini Flate, predictor PNG e JPEG, verificati nei pixel. Sanitizzatori ASan/UBSan e conversioni float; 250 input troncati/alterati. I test non garantiscono compatibilità con tutti i PDF.

Fixture riproducibili: `python3 native/tests/pdf_native_fixtures.py`. Test: `native/tests/pdf_native_test.c`, `pdf_native_fuzz.c` e self-test della shell. Riferimento primario: [Adobe PDF Reference 1.7](https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf).
