# Desktop PDF

Renderer C sviluppato per Desktop Mode, API `pdf_engine.h`. Dipendenze disponibili in VitaSDK: zlib (Flate), FreeType (glyph rasterization), libjpeg (DCT images). Non e un'implementazione completa di ISO 32000: il supporto verificato e descritto in docs/pdf-viewer.md. Le caratteristiche non supportate causano un errore esplicito: non presentare un'immagine incompleta come rendering corretto.

Riferimento di formato: Adobe PDF Reference 1.7 / ISO 32000-1, https://opensource.adobe.com/dc-acrobat-sdk-docs/pdfstandards/pdfreference1.7old.pdf .
