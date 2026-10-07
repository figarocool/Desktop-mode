# Icone trasparenti

`icons-transparent.png` è un atlas RGBA 4×2 generato con il tool integrato imagegen a partire da `/home/stefano/Scaricati/desktop-mode.jpg`. La shell usa otto regioni dell'atlas, preservandone il canale alfa. Il PNG è salvato nel progetto e non dipende dalla cartella di generazione.

Prompt finale:

> Use case: background-extraction. Asset type: one desktop icon atlas PNG sprite sheet with actual transparent alpha. Input image is reference for the eight desktop icons. Create a single 4-column by 2-row evenly spaced atlas on a transparent background, no labels, no background squares. Row 1 left to right: colorful PlayStation PS symbol, silver console tower with controller (computer), silver device panel with controller (devices), parchment document scroll (documents). Row 2 left to right: white recycle bin with blue recycling mark, blue games folder with PlayStation triangle circle cross square, blue network globe with PSN, blue/gold internet e symbol. Keep the Vista-era glossy dimensional icon look and shapes from the reference, remove the blue wallpaper behind every icon completely. Each icon centered in its identical cell with generous clear margins, none crossing cell boundaries. Atlas must have exactly those 8 icons in that order and no text beyond PSN logo. Preserve the reference appearance as closely as possible.

Le icone ricreano il riferimento ma non sono gli asset originali. I file `wallpaper0.png`–`wallpaper2.png` e `icon0.png` sono quelli del primo prototipo C.
