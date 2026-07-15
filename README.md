Poker-Engine-Wasm

Un motore di gioco Texas Hold'em ad alte prestazioni scritto in linguaggio C, progettato per essere compilato in WebAssembly (Wasm).

Il progetto segue un'architettura modulare che separa nettamente il motore logico (scritto in C puro) dall'interfaccia utente (HTML/JS), permettendo di eseguire la logica di gioco in modo nativo all'interno del browser.

Caratteristiche principali
- Core in C: Motore logico robusto, manutenibile e ottimizzato per le performance.
- Web-Ready: Progettato per l'integrazione via Emscripten per l'esecuzione in ambiente browser.
- Architettura Modulare: Separazione netta tra la gestione dei dati (mazzo, giocatori) e la logica di gioco (valutatore mani, gestione puntate).
- Cross-platform: Può essere compilato sia come eseguibile CLI standard che come modulo Wasm.

Struttura del Progetto
- `/src`: Codice sorgente in C (`deck.c`, `rules.c`, `main.c`, ecc.).
- `/include`: File header (`.h`) per la definizione delle interfacce.
- `/web`: File `index.html` e `script.js` per l'interfaccia web.
- `Makefile`: Automazione della compilazione per ambiente nativo.

Requisiti
Per la compilazione verso WebAssembly, è necessario il toolkit [Emscripten](https://emscripten.org/).

Come compilare
Versione Nativa (CLI)
```bash
make
./poker_engine
