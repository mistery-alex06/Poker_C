#include <stdio.h>
#include <string.h>
#include "hand_eval.h"

typedef struct {
    int valore;
    int count;
} Gruppo;

/* Ordina gruppi per count desc, poi valore desc (insertion sort, n piccolo) */
static void ordina_gruppi(Gruppo *g, int n) {
    for (int i = 1; i < n; i++) {
        Gruppo tmp = g[i];
        int j = i - 1;
        while (j >= 0 && (g[j].count < tmp.count ||
                          (g[j].count == tmp.count && g[j].valore < tmp.valore))) {
            g[j + 1] = g[j];
            j--;
        }
        g[j + 1] = tmp;
    }
}

static unsigned long calcola_punteggio(TipoMano tipo, const int tiebreak[5]) {
    unsigned long p = (unsigned long)tipo;
    for (int i = 0; i < 5; i++) {
        p = p * 15UL + (unsigned long)tiebreak[i];
    }
    return p;
}

RisultatoMano valuta_mano5(const Carta mano5[5]) {
    RisultatoMano r;
    memcpy(r.migliori_carte, mano5, sizeof(Carta) * 5);

    int conta[15] = {0};
    int semi_uguali = 1;
    for (int i = 0; i < 5; i++) {
        conta[mano5[i].valore]++;
        if (mano5[i].seme != mano5[0].seme) semi_uguali = 0;
    }

    Gruppo gruppi[15];
    int n_gruppi = 0;
    for (int v = 14; v >= 2; v--) {
        if (conta[v] > 0) {
            gruppi[n_gruppi].valore = v;
            gruppi[n_gruppi].count = conta[v];
            n_gruppi++;
        }
    }
    ordina_gruppi(gruppi, n_gruppi);

    int tiebreak[5] = {0, 0, 0, 0, 0};
    TipoMano tipo;

    if (n_gruppi == 5) {
        /* valori distinti, gia' ordinati desc perche' costruiti da v=14..2 */
        int valori[5];
        for (int i = 0; i < 5; i++) valori[i] = gruppi[i].valore;

        int scala = (valori[0] - valori[4] == 4);
        int high_scala = valori[0];
        /* wheel: A,5,4,3,2 */
        if (!scala && valori[0] == 14 && valori[1] == 5 && valori[2] == 4 &&
            valori[3] == 3 && valori[4] == 2) {
            scala = 1;
            high_scala = 5;
        }

        if (scala && semi_uguali) {
            tipo = SCALA_COLORE;
            tiebreak[0] = high_scala;
        } else if (semi_uguali) {
            tipo = COLORE;
            for (int i = 0; i < 5; i++) tiebreak[i] = valori[i];
        } else if (scala) {
            tipo = SCALA;
            tiebreak[0] = high_scala;
        } else {
            tipo = HIGH_CARD;
            for (int i = 0; i < 5; i++) tiebreak[i] = valori[i];
        }
    } else if (n_gruppi == 4) {
        tipo = COPPIA;
        for (int i = 0; i < 4; i++) tiebreak[i] = gruppi[i].valore;
    } else if (n_gruppi == 3) {
        if (gruppi[0].count == 3) {
            tipo = TRIS;
            tiebreak[0] = gruppi[0].valore;
            tiebreak[1] = gruppi[1].valore;
            tiebreak[2] = gruppi[2].valore;
        } else {
            tipo = DOPPIA_COPPIA;
            tiebreak[0] = gruppi[0].valore;
            tiebreak[1] = gruppi[1].valore;
            tiebreak[2] = gruppi[2].valore;
        }
    } else { /* n_gruppi == 2 */
        if (gruppi[0].count == 4) {
            tipo = POKER;
            tiebreak[0] = gruppi[0].valore;
            tiebreak[1] = gruppi[1].valore;
        } else {
            tipo = FULL_HOUSE;
            tiebreak[0] = gruppi[0].valore;
            tiebreak[1] = gruppi[1].valore;
        }
    }

    r.tipo = tipo;
    r.punteggio = calcola_punteggio(tipo, tiebreak);
    return r;
}

/* Genera combinazioni di 5 indici su n carte, valuta ciascuna, tiene la migliore */
RisultatoMano valuta_migliore_mano(const Carta carte[], int n) {
    RisultatoMano migliore;
    migliore.punteggio = 0;
    migliore.tipo = HIGH_CARD;

    if (n < 5) {
        /* fallback: non abbastanza carte, valuta cio' che c'e' azzerando il resto */
        Carta tmp[5] = {0};
        for (int i = 0; i < n && i < 5; i++) tmp[i] = carte[i];
        return valuta_mano5(tmp);
    }

    int idx[5];
    for (int i = 0; i < 5; i++) idx[i] = i;

    int primo = 1;
    while (1) {
        Carta mano5[5];
        for (int i = 0; i < 5; i++) mano5[i] = carte[idx[i]];

        RisultatoMano r = valuta_mano5(mano5);
        if (primo || r.punteggio > migliore.punteggio) {
            migliore = r;
            primo = 0;
        }

        /* Prossima combinazione (algoritmo standard next-combination) */
        int i = 4;
        while (i >= 0 && idx[i] == n - 5 + i) i--;
        if (i < 0) break;
        idx[i]++;
        for (int j = i + 1; j < 5; j++) idx[j] = idx[j - 1] + 1;
    }

    return migliore;
}

int confronta_mani(const RisultatoMano *a, const RisultatoMano *b) {
    if (a->punteggio > b->punteggio) return 1;
    if (a->punteggio < b->punteggio) return -1;
    return 0;
}

const char *tipo_mano_to_string(TipoMano t) {
    switch (t) {
        case HIGH_CARD:     return "Carta Alta";
        case COPPIA:        return "Coppia";
        case DOPPIA_COPPIA: return "Doppia Coppia";
        case TRIS:          return "Tris";
        case SCALA:         return "Scala";
        case COLORE:        return "Colore";
        case FULL_HOUSE:    return "Full House";
        case POKER:         return "Poker";
        case SCALA_COLORE:  return "Scala Colore";
        default:            return "???";
    }
}
