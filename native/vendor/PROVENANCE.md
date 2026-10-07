# Dipendenze aggiunte

libsmb2: https://github.com/sahlberg/libsmb2/tree/51c5910da240a3b60bc4ad1b472185c2815628d4
Snapshot codeload ricevuto il 2026-10-05, commit verificato nell’intestazione PAX. SHA-256 archivio: `9562373e10c8e1cc7360dab51b62e0ee6bb443b4b6c894838f33a2fd54747131`. Sorgenti e licenze conservati integralmente. Modifica locale: `lib/init.c` usa `sceKernelGetRandomNumber` su Vita per i byte casuali. Il core è LGPL-2.1-or-later; DCE/RPC separato non viene compilato.

Browser: libcurl/libxml2/OpenSSL/zlib/zstd forniti da VitaSDK; l’anteprima usa le librerie di sistema. Il bundle `assets/cacert.pem` proviene da `/etc/ssl/certs/ca-certificates.crt` della macchina di sviluppo, copiato il 2026-10-05; mantenere aggiornate le CA per la compatibilità HTTPS.
