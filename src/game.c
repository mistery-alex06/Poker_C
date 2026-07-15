#include <stdio.h>
#include <string.h>
#include "game.h"

void tavolo_init(Tavolo *t, int n_giocatori, const char *nomi[], int stack_iniziale,
                  int piccolo_buio, int grande_buio) {
    t->n_giocatori = (n_giocatori > MAX_GIOCATORI) ? MAX_GIOCATORI : n_giocatori;
    for (int i = 0; i < t->n_giocatori; i++) {
        player_init(&t->giocatori[i], nomi[i], stack_iniziale, i);
    }
    t->piccolo_buio = piccolo_buio;
    t->grande_buio = grande_buio;
    t->dealer = -1; /* la prima tavolo_nuova_mano lo porta a 0 */
    t->n_board = 0;
    t->piatto = 0;
    t->puntata_massima = 0;
    t->fase = FASE_PREFLOP;
}

int tavolo_conta_attivi_in_mano(const Tavolo *t) {
    int n = 0;
    for (int i = 0; i < t->n_giocatori; i++) {
        StatoGiocatore s = t->giocatori[i].stato;
        if (s == ATTIVO || s == ALL_IN) n++;
    }
    return n;
}

static int conta_devono_agire(const Tavolo *t) {
    int n = 0;
    for (int i = 0; i < t->n_giocatori; i++) {
        if (t->giocatori[i].stato == ATTIVO) n++;
    }
    return n;
}

void tavolo_nuova_mano(Tavolo *t) {
    deck_init(&t->mazzo);
    deck_shuffle(&t->mazzo);

    t->n_board = 0;
    t->piatto = 0;
    t->puntata_massima = 0;
    t->fase = FASE_PREFLOP;
    t->dealer = (t->dealer + 1) % t->n_giocatori;

    for (int i = 0; i < t->n_giocatori; i++) {
        player_reset_mano(&t->giocatori[i]);
    }

    /* distribuzione a giro (2 carte a testa) */
    for (int giro = 0; giro < 2; giro++) {
        for (int i = 0; i < t->n_giocatori; i++) {
            if (t->giocatori[i].stato != ELIMINATO) {
                player_assegna_carta(&t->giocatori[i], giro, *deck_draw(&t->mazzo));
            }
        }
    }

    /* bui: semplificazione, assume almeno 2 giocatori non eliminati consecutivi */
    int sb_idx = (t->dealer + 1) % t->n_giocatori;
    int bb_idx = (t->dealer + 2) % t->n_giocatori;

    int puntato_sb = player_punta(&t->giocatori[sb_idx], t->piccolo_buio);
    int puntato_bb = player_punta(&t->giocatori[bb_idx], t->grande_buio);
    t->piatto += puntato_sb + puntato_bb;
    t->puntata_massima = t->giocatori[bb_idx].puntata_corrente;
}

static void reset_puntate_giro(Tavolo *t) {
    for (int i = 0; i < t->n_giocatori; i++) {
        t->giocatori[i].puntata_corrente = 0;
    }
    t->puntata_massima = 0;
}

void tavolo_rivela_flop(Tavolo *t) {
    deck_draw(&t->mazzo); /* brucia una carta */
    for (int i = 0; i < 3; i++) {
        t->board[t->n_board++] = *deck_draw(&t->mazzo);
    }
    reset_puntate_giro(t);
    t->fase = FASE_FLOP;
}

void tavolo_rivela_turn(Tavolo *t) {
    deck_draw(&t->mazzo);
    t->board[t->n_board++] = *deck_draw(&t->mazzo);
    reset_puntate_giro(t);
    t->fase = FASE_TURN;
}

void tavolo_rivela_river(Tavolo *t) {
    deck_draw(&t->mazzo);
    t->board[t->n_board++] = *deck_draw(&t->mazzo);
    reset_puntate_giro(t);
    t->fase = FASE_RIVER;
}

void tavolo_giro_puntate(Tavolo *t, FunzioneDecisione decidi, int primo_giocatore) {
    int n = t->n_giocatori;
    if (tavolo_conta_attivi_in_mano(t) <= 1) return;

    int da_agire = conta_devono_agire(t);
    if (da_agire == 0) return; /* tutti gia' all-in, niente da decidere */

    int idx = primo_giocatore % n;
    int iter = 0;
    int max_iter = n * 20; /* guardia di sicurezza anti-loop-infinito */

    while (da_agire > 0 && iter < max_iter) {
        iter++;
        Giocatore *g = &t->giocatori[idx];

        if (g->stato != ATTIVO) {
            idx = (idx + 1) % n;
            continue;
        }

        int da_pareggiare = t->puntata_massima - g->puntata_corrente;
        int importo = 0;
        Azione a = decidi(g, da_pareggiare, t->grande_buio, &importo);
        int era_rilancio = 0;

        switch (a) {
            case AZIONE_FOLD:
                player_fold(g);
                break;

            case AZIONE_CHECK:
                if (da_pareggiare > 0) player_fold(g); /* check non valido: fold difensivo */
                break;

            case AZIONE_CALL:
                t->piatto += player_punta(g, da_pareggiare);
                break;

            case AZIONE_RAISE: {
                int punta_minima = t->puntata_massima + t->grande_buio;
                int punta_a = (importo > punta_minima) ? importo : punta_minima;
                int necessario = punta_a - g->puntata_corrente;
                t->piatto += player_punta(g, necessario);
                if (g->puntata_corrente > t->puntata_massima) {
                    t->puntata_massima = g->puntata_corrente;
                    era_rilancio = 1;
                }
                break;
            }
        }

        if (era_rilancio) {
            int altri_attivi = 0;
            for (int i = 0; i < n; i++) {
                if (i != idx && t->giocatori[i].stato == ATTIVO) altri_attivi++;
            }
            da_agire = altri_attivi;
        } else {
            da_agire--;
        }

        idx = (idx + 1) % n;

        if (tavolo_conta_attivi_in_mano(t) <= 1) break;
    }
}

void tavolo_assegna_piatto(Tavolo *t) {
    /* NOTA: piatto unico, non gestisce side-pot per all-in con stack diversi */
    int vincenti[MAX_GIOCATORI];
    int n_vincenti = 0;
    unsigned long miglior_punteggio = 0;

    for (int i = 0; i < t->n_giocatori; i++) {
        Giocatore *g = &t->giocatori[i];
        if (g->stato != ATTIVO && g->stato != ALL_IN) continue;

        Carta carte[7];
        carte[0] = g->mano[0];
        carte[1] = g->mano[1];
        for (int j = 0; j < t->n_board; j++) carte[2 + j] = t->board[j];

        RisultatoMano r = valuta_migliore_mano(carte, 2 + t->n_board);

        if (n_vincenti == 0 || r.punteggio > miglior_punteggio) {
            miglior_punteggio = r.punteggio;
            vincenti[0] = i;
            n_vincenti = 1;
        } else if (r.punteggio == miglior_punteggio) {
            vincenti[n_vincenti++] = i;
        }
    }

    if (n_vincenti == 0) return;

    int quota = t->piatto / n_vincenti;
    int resto = t->piatto % n_vincenti;
    for (int k = 0; k < n_vincenti; k++) {
        t->giocatori[vincenti[k]].stack += quota;
    }
    t->giocatori[vincenti[0]].stack += resto; /* resto al primo vincente in ordine di seat */

    t->piatto = 0;
    t->fase = FASE_SHOWDOWN;
}

void tavolo_stampa_board(const Tavolo *t) {
    char buf[32];
    printf("Board: ");
    for (int i = 0; i < t->n_board; i++) {
        carta_to_string(&t->board[i], buf, sizeof(buf));
        printf("[%s] ", buf);
    }
    printf("\n");
}
