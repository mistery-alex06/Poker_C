#ifndef HAND_EVAL_H
#define HAND_EVAL_H

#include "deck.h"

typedef enum {
    HIGH_CARD = 0,
    COPPIA,
    DOPPIA_COPPIA,
    TRIS,
    SCALA,
    COLORE,
    FULL_HOUSE,
    POKER,
    SCALA_COLORE   /* include anche la Scala Reale (Asso alto) */
} TipoMano;

typedef struct {
    TipoMano tipo;
    unsigned long punteggio;   /* score comparabile: tipo + tiebreak */
    Carta migliori_carte[5];
} RisultatoMano;

/* Valuta esattamente 5 carte */
RisultatoMano valuta_mano5(const Carta mano5[5]);

/* Valuta la miglior combinazione di 5 carte tra n (2<=n<=7) */
RisultatoMano valuta_migliore_mano(const Carta carte[], int n);

/* Confronta due risultati: >0 se a vince, <0 se b vince, 0 pareggio */
int confronta_mani(const RisultatoMano *a, const RisultatoMano *b);

const char *tipo_mano_to_string(TipoMano t);

#endif /* HAND_EVAL_H */
