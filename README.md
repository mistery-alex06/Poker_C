# Poker-Engine-Wasm

Un motore di gioco Texas Hold'em scritto in C puro, con interfaccia browser compilata in WebAssembly.

Il progetto separa nettamente il motore logico (C) dall'interfaccia (HTML/CSS/JS): tutte le regole del gioco — distribuzione carte, gestione bui, giri di puntata, valutazione mani, assegnazione piatto — vivono nel motore C. Il JavaScript si limita a chiamare le funzioni esposte e a disegnare lo stato che riceve.

## Come si gioca

### Il tavolo

Si gioca in **3**: tu (`Tu`, sempre seduto al seat 0) contro due bot (`Bot 1`, `Bot 2`). Ogni giocatore parte con **1000 chip**. I bui sono fissi: piccolo buio **10**, grande buio **20**. Il bottone del dealer ruota di un posto ad ogni nuova mano.

### Svolgimento di una mano

1. **Preflop**: vengono distribuite 2 carte coperte (*hole cards*) a testa. Il piccolo buio e il grande buio vengono posizionati automaticamente. Si parte a puntare dal giocatore dopo il grande buio.
2. **Flop**: vengono scoperte le prime 3 carte comuni al centro del tavolo, poi un nuovo giro di puntate.
3. **Turn**: una quarta carta comune, poi un altro giro di puntate.
4. **River**: la quinta e ultima carta comune, poi l'ultimo giro di puntate.
5. **Showdown**: chi è ancora in mano mostra le carte; vince chi forma la combinazione più alta usando **le migliori 5 carte tra le proprie 2 coperte e le 5 comuni**. In caso di parità il piatto viene diviso.

Se durante un giro di puntate restano attivi meno di 2 giocatori (tutti gli altri hanno fatto fold), la mano finisce subito e il piatto va all'unico rimasto, senza bisogno di arrivare al river.

### Le azioni disponibili

| Azione | Quando è disponibile | Effetto |
|---|---|---|
| **Check** | Solo se non c'è nessuna puntata da pareggiare | Passi il turno senza mettere chip |
| **Call** | Solo se c'è una puntata da pareggiare | Eguagli la puntata massima corrente |
| **Fold** | Sempre | Ti ritiri dalla mano, perdi quanto già puntato |
| **Raise** | Sempre (salvo il tetto rilanci, vedi sotto) | Rilanci **oltre** la puntata massima corrente |

**Il rilancio è "Raise By", non "Raise To"**: l'importo che inserisci nel campo numerico è quanto vuoi aggiungere sopra la puntata massima già sul tavolo, non il totale a cui vuoi arrivare. Esempio: se la puntata massima è 20 e inserisci 30, la nuova puntata massima diventa **50** (20 + 30), non 30. Il rilancio minimo consentito è pari al grande buio (20); se inserisci meno, viene arrotondato al minimo.

Per evitare partite infinite, **non si possono fare più di 4 rilanci nello stesso giro di puntate**: superato il tetto, anche i bot possono solo call/check/fold, così il gioco è sempre garantito ad avanzare.

### Classifica delle mani (dalla più debole alla più forte)

1. Carta Alta
2. Coppia
3. Doppia Coppia
4. Tris
5. Scala (incluso il "wheel" A-2-3-4-5, dove l'Asso vale come 1)
6. Colore
7. Full House
8. Poker
9. Scala Colore (la Scala Reale è semplicemente la Scala Colore più alta, Asso in cima)

### I bot

I due bot non seguono ciecamente il giocatore umano: valutano la forza della propria mano (in base alle carte comuni già scoperte), la posizione al tavolo e quanto piatto stanno già "impegnando" prima di decidere. Rilanciano con più convinzione quando la mano è forte, foldano quando non ne vale la pena. Il sizing dei rilanci è moderato (tipicamente 2-4x il grande buio, mai più di un quarto del proprio stack in un colpo), per restare competitivi senza essere temerari o rimanere bloccati in scambi di rilanci infiniti.

## Caratteristiche tecniche

- **Core in C puro**: motore di gioco robusto e testabile, compilabile sia nativamente (CLI) che in WebAssembly.
- **Valutatore di mani completo**: riconosce tutte le combinazioni, incluso il wheel e gli spareggi per carte alte/kicker.
- **Puntate "Raise By"** con validazione del rilancio minimo e tetto anti-loop di 4 rilanci per giro.
- **Bot con strategia**: sia il motore CLI (`main.c`) che quello del browser (`wasm_bridge.c`) valutano forza mano, posizione al tavolo e pot commitment.
- **Interfaccia web premium**: tavolo ovale, carte animate, registro azioni in tempo reale con esito dello showdown, ritmo dei turni scandito da pause realistiche (i bot "ci pensano" per un tempo variabile invece di agire all'istante).
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
