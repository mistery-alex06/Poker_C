# Poker-Engine-Wasm

Un motore di gioco Texas Hold'em scritto in C puro, con interfaccia browser compilata in WebAssembly.

Il progetto separa nettamente il motore logico (C) dall'interfaccia (HTML/CSS/JS): tutte le regole del gioco — distribuzione carte, gestione bui, giri di puntata, valutazione mani, assegnazione piatto — vivono nel motore C. Il JavaScript si limita a chiamare le funzioni esposte e a disegnare lo stato che riceve.

## Caratteristiche principali

- **Core in C puro**: motore di gioco robusto e testabile, compilabile sia nativamente (CLI) che in WebAssembly.
- **Valutatore di mani completo**: riconosce tutte le combinazioni (coppia → scala colore), incluso il wheel (A-2-3-4-5) e gli spareggi per carte alte/kicker.
- **Puntate "Raise By"**: il rilancio si esprime come incremento sopra la puntata massima corrente (non come totale assoluto), con validazione del rilancio minimo.
- **Bot con strategia**: sia il motore CLI (`main.c`) che quello del browser (`wasm_bridge.c`) valutano forza mano, posizione al tavolo e pot commitment prima di decidere fold/check/call/raise — non seguono semplicemente il giocatore umano.
- **Interfaccia web premium**: tavolo ovale, carte animate, registro azioni in tempo reale con esito dello showdown, ritmo dei turni scandito da pause realistiche.
- **Zero server richiesto**: grazie a `SINGLE_FILE=1`, il modulo Wasm è incorporato in `poker.js` — `web/index.html` si apre con un doppio click.

## Struttura del progetto

```
Poker_C/
├── Makefile              # build nativo + build Wasm
├── include/               # header (.h)
│   ├── deck.h              # carte, semi, mazzo
│   ├── player.h             # giocatore, stati (attivo/fold/all-in/eliminato)
│   ├── hand_eval.h           # valutazione mani
│   ├── game.h                # tavolo, fasi, motore puntate
│   └── wasm_bridge.h          # API esposta al browser
├── src/                   # implementazione
│   ├── deck.c / player.c / hand_eval.c / game.c
│   ├── main.c              # eseguibile CLI con bot di test
│   └── wasm_bridge.c        # bridge per il browser (bot con strategia)
└── web/                   # interfaccia browser
    ├── index.html
    ├── style.css
    ├── script.js            # solo orchestrazione, nessuna regola di gioco
    └── poker.js             # generato da `make wasm` (non versionato)
```

## Come compilare

### Versione nativa (CLI)

```bash
make
./bin/poker
```

Oppure, in un solo passaggio:

```bash
make run
```

Simula una mano completa a 3 giocatori (Alice/Bob/Carol) contro bot con strategia, stampando ogni fase su terminale.

### Versione browser (WebAssembly)

Richiede il toolkit [Emscripten](https://emscripten.org/docs/getting_started/downloads.html) installato e attivato:

```bash
source /path/to/emsdk/emsdk_env.sh
make wasm
```

Genera `web/poker.js` (include il modulo Wasm incorporato). A quel punto basta aprire `web/index.html` con un doppio click: nessun server locale necessario.

Per ripulire i build:

```bash
make clean        # rimuove obj/ e bin/
make clean-wasm   # rimuove web/poker.js
```

## Note tecniche

- Piatto unico: non sono gestiti i side-pot per all-in parziali con stack diversi.
- Le carte dei bot restano oscurate (`??`) nel JSON di stato finché non è showdown.
- Il motore `wasm_avanza()` avanza di un passo atomico per chiamata (un'azione di un giocatore, oppure una transizione di fase), così l'interfaccia può scandire i turni con pause realistiche invece di risolverli tutti insieme.
