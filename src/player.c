#include <string.h>
#include "player.h"

void player_init(Giocatore *g, const char *nome, int stack_iniziale, int posizione) {
    strncpy(g->nome, nome, NOME_MAX_LEN - 1);
    g->nome[NOME_MAX_LEN - 1] = '\0';
    g->stack = stack_iniziale;
    g->posizione = posizione;
    player_reset_mano(g);
}

void player_reset_mano(Giocatore *g) {
    g->puntata_corrente = 0;
    g->puntata_totale_mano = 0;
    g->stato = (g->stack > 0) ? ATTIVO : ELIMINATO;
}

void player_assegna_carta(Giocatore *g, int idx, Carta c) {
    if (idx >= 0 && idx < CARTE_IN_MANO) {
        g->mano[idx] = c;
    }
}

int player_punta(Giocatore *g, int importo) {
    int reale = (importo > g->stack) ? g->stack : importo;
    g->stack -= reale;
    g->puntata_corrente += reale;
    g->puntata_totale_mano += reale;
    if (g->stack == 0 && g->stato == ATTIVO) {
        g->stato = ALL_IN;
    }
    return reale;
}

void player_fold(Giocatore *g) {
    g->stato = FOLD;
}

int player_e_attivo(const Giocatore *g) {
    return g->stato == ATTIVO || g->stato == ALL_IN;
}
