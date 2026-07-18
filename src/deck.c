#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "deck.h"

void deck_init(Mazzo *mazzo) {
    int idx = 0;
    for (int s = 0; s < NUM_SEMI; s++) {
        for (int v = DUE; v <= ASSO; v++) {
            mazzo->carte[idx].seme = (Seme)s;
            mazzo->carte[idx].valore = (Valore)v;
            idx++;
        }
    }
    mazzo->n_carte = NUM_CARTE;
    mazzo->top = 0;
}

void deck_reset(Mazzo *mazzo) {
    deck_init(mazzo);
}

void deck_shuffle(Mazzo *mazzo) {
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    /* Fisher-Yates: mescola solo le carte non ancora pescate [top, n_carte-1] */
    for (int i = mazzo->n_carte - 1; i > mazzo->top; i--) {
        int j = mazzo->top + rand() % (i - mazzo->top + 1);
        Carta tmp = mazzo->carte[i];
        mazzo->carte[i] = mazzo->carte[j];
        mazzo->carte[j] = tmp;
    }
}

Carta *deck_draw(Mazzo *mazzo) {
    if (mazzo->top >= mazzo->n_carte) {
        return NULL; /* mazzo esaurito */
    }
    return &mazzo->carte[mazzo->top++];
}

const char *seme_to_string(Seme s) {
    switch (s) {
        case QUADRI: return "Quadri";
        case CUORI:  return "Cuori";
        case FIORI:  return "Fiori";
        case PICCHE: return "Picche";
        default:     return "???";
    }
}

const char *valore_to_string(Valore v) {
    switch (v) {
        case DUE:   return "2";
        case TRE:   return "3";
        case QUATTRO: return "4";
        case CINQUE:  return "5";
        case SEI:     return "6";
        case SETTE:   return "7";
        case OTTO:    return "8";
        case NOVE:    return "9";
        case DIECI:   return "10";
        case JACK:    return "J";
        case QUEEN:   return "Q";
        case KING:    return "K";
        case ASSO:    return "A";
        default:      return "?";
    }
}

void carta_to_string(const Carta *c, char *buffer, size_t buffer_size) {
    snprintf(buffer, buffer_size, "%s di %s", valore_to_string(c->valore), seme_to_string(c->seme));
}
