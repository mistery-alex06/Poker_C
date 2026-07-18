#ifndef GAME_H
#define GAME_H

#include "deck.h"
#include "player.h"
#include "hand_eval.h"

#define MAX_GIOCATORI 9

typedef enum {
    FASE_PREFLOP = 0,
    FASE_FLOP,
    FASE_TURN,
    FASE_RIVER,
    FASE_SHOWDOWN
} FaseGioco;

typedef enum {
    AZIONE_FOLD = 0,
    AZIONE_CHECK,
    AZIONE_CALL,
    AZIONE_RAISE
} Azione;

/* Callback di decisione: dato il giocatore e quanto deve pareggiare,
 * ritorna l'azione scelta. Se AZIONE_RAISE, *importo_puntato deve
 * contenere l'incremento "raise by" oltre la puntata massima corrente
 * (non il totale assoluto della puntata). */
typedef Azione (*FunzioneDecisione)(Giocatore *g, int da_pareggiare, int puntata_minima, int *importo_puntato);

typedef struct {
    Giocatore giocatori[MAX_GIOCATORI];
    int n_giocatori;
    Mazzo mazzo;
    Carta board[5];
    int n_board;
    int piatto;
    int puntata_massima;   /* puntata corrente da pareggiare nel giro attivo */
    int dealer;            /* indice giocatore col bottone */
    int piccolo_buio;
    int grande_buio;
    FaseGioco fase;
    int showdown_vincitore;
    int showdown_n_vincitori;
    int showdown_vincitori[MAX_GIOCATORI];
    TipoMano showdown_tipo;
    unsigned long showdown_punteggio;
} Tavolo;

void tavolo_init(Tavolo *t, int n_giocatori, const char *nomi[], int stack_iniziale,
                  int piccolo_buio, int grande_buio);

/* Prepara una nuova mano: reset stati, mescola, distribuisce, posta i bui */
void tavolo_nuova_mano(Tavolo *t);

void tavolo_rivela_flop(Tavolo *t);
void tavolo_rivela_turn(Tavolo *t);
void tavolo_rivela_river(Tavolo *t);

/* Numero giocatori non in FOLD ne' ELIMINATO */
int tavolo_conta_attivi_in_mano(const Tavolo *t);

/* Esegue un giro di puntate partendo da primo_giocatore (indice in giocatori[]).
 * Ritorna quando tutti gli attivi hanno pareggiato la puntata massima o sono fold/all-in. */
void tavolo_giro_puntate(Tavolo *t, FunzioneDecisione decidi, int primo_giocatore);

/* Showdown: valuta le mani degli attivi, assegna il piatto (diviso in caso di parita').
 * NOTA: gestisce un unico piatto principale, non side-pot per all-in parziali. */
void tavolo_assegna_piatto(Tavolo *t);

void tavolo_stampa_board(const Tavolo *t);

#endif /* GAME_H */
