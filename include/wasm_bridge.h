#ifndef WASM_BRIDGE_H
#define WASM_BRIDGE_H

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif

/* Seat 0 e' sempre il giocatore umano; i restanti sono bot gestiti in C. */
#define HUMAN_SEAT 0

/* Crea il tavolo (3 giocatori, stack 1000, bui 10/20). Chiamare una sola volta. */
EMSCRIPTEN_KEEPALIVE void wasm_crea_partita(void);

/* Inizia una nuova mano: mescola, distribuisce, posta i bui, avvia il preflop. */
EMSCRIPTEN_KEEPALIVE void wasm_nuova_mano(void);

/* Fa avanzare la partita di UN passo atomico e ritorna:
 *   HUMAN_SEAT (0)     -> in attesa di un'azione umana (JS deve mostrare i controlli)
 *   1, 2, ...          -> indice del bot che ha appena agito (chiamare di nuovo per proseguire)
 *   -1                 -> mano conclusa (showdown fatto, piatto assegnato)
 *   -2                 -> fase avanzata (flop/turn/river appena rivelati, chiamare di nuovo)
 */
EMSCRIPTEN_KEEPALIVE int wasm_avanza(void);

/* Applica l'azione del giocatore umano (chiamare solo dopo che wasm_avanza
 * ha ritornato HUMAN_SEAT). azione: 0=FOLD 1=CHECK 2=CALL 3=RAISE.
 * importo e' usato solo per RAISE: e' l'incremento "raise by" da sommare
 * alla puntata massima corrente (non la puntata totale desiderata). */
EMSCRIPTEN_KEEPALIVE void wasm_azione_umano(int azione, int importo);

/* Ritorna lo stato corrente del gioco come stringa JSON (buffer statico interno).
 * Le carte dei bot sono oscurate ("??") finche' non e' showdown. */
EMSCRIPTEN_KEEPALIVE const char *wasm_stato_json(void);

#endif /* WASM_BRIDGE_H */
