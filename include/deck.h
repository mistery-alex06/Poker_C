#ifndef DECK_H
#define DECK_H

#include <stddef.h>

#define NUM_SEMI 4
#define NUM_VALORI 13
#define NUM_CARTE 52

typedef enum {
    QUADRI = 0,
    CUORI  = 1,
    FIORI  = 2,
    PICCHE = 3
} Seme;

typedef enum {
    DUE = 2,
    TRE,
    QUATTRO,
    CINQUE,
    SEI,
    SETTE,
    OTTO,
    NOVE,
    DIECI,
    JACK,
    QUEEN,
    KING,
    ASSO
} Valore;

typedef struct {
    Seme seme;
    Valore valore;
} Carta;

typedef struct {
    Carta carte[NUM_CARTE];
    int n_carte;      /* carte rimaste da pescare */
    int top;          /* indice prossima carta da pescare */
} Mazzo;

/* Inizializza il mazzo con le 52 carte in ordine */
void deck_init(Mazzo *mazzo);

/* Fisher-Yates shuffle */
void deck_shuffle(Mazzo *mazzo);

/* Pesca una carta dal mazzo, ritorna NULL se esaurito */
Carta *deck_draw(Mazzo *mazzo);

/* Reset mazzo (ricarica le 52 carte, non mescolate) */
void deck_reset(Mazzo *mazzo);

/* Stringhe leggibili */
const char *seme_to_string(Seme s);
const char *valore_to_string(Valore v);
void carta_to_string(const Carta *c, char *buffer, size_t buffer_size);

#endif /* DECK_H */
