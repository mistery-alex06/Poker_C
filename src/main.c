#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "game.h"

static Tavolo *decidi_tavolo = NULL;

static int valuta_forza_bot(const Giocatore *g, RisultatoMano *out) {
    if (!decidi_tavolo) {
        if (out) { out->punteggio = 0; out->tipo = HIGH_CARD; }
        return HIGH_CARD;
    }

    Carta carte[7];
    carte[0] = g->mano[0];
    carte[1] = g->mano[1];
    for (int i = 0; i < decidi_tavolo->n_board; i++) {
        carte[2 + i] = decidi_tavolo->board[i];
    }

    RisultatoMano r = valuta_migliore_mano(carte, 2 + decidi_tavolo->n_board);
    if (out) *out = r;
    return (int)r.tipo;
}

/* Bot migliorato: strategie per fase, posizione, e considerer stack/pot ratio */
static int conta_giocatori_attivi_dopo_hero(const Tavolo *t, int hero_idx) {
    int cnt = 0;
    for (int i = 0; i < t->n_giocatori; i++) {
        if (i != hero_idx && t->giocatori[i].stato == ATTIVO) cnt++;
    }
    return cnt;
}

static int hero_posizione_iniziale(const Tavolo *t, int hero_idx) {
    int distance = (hero_idx - t->dealer + t->n_giocatori) % t->n_giocatori;
    if (distance == 0) return 0;  /* dealer */
    if (distance == 1) return 1;  /* small blind */
    if (distance == 2) return 2;  /* big blind */
    if (distance <= t->n_giocatori / 2) return 3;  /* early/middle */
    return 4;  /* late/button */
}

static int ha_draw_forte(const Giocatore *g) {
    if (!decidi_tavolo || decidi_tavolo->n_board < 3) return 0;
    
    Carta carte[7];
    carte[0] = g->mano[0];
    carte[1] = g->mano[1];
    for (int i = 0; i < decidi_tavolo->n_board; i++) {
        carte[2 + i] = decidi_tavolo->board[i];
    }

    int semi_uguali = 0;
    for (int s = 0; s < 4; s++) {
        int cnt = 0;
        for (int i = 0; i < 2 + decidi_tavolo->n_board; i++) {
            if ((int)carte[i].seme == s) cnt++;
        }
        if (cnt >= 4) semi_uguali = 1;
    }

    int consecutive = 0;
    for (int v = 2; v < 12; v++) {
        int cnt = 0;
        for (int i = 0; i < 2 + decidi_tavolo->n_board; i++) {
            int val = carte[i].valore;
            if ((val >= v && val <= v + 4) || (val == ASSO && v == 2)) cnt++;
        }
        if (cnt >= 4) consecutive = 1;
    }

    return semi_uguali || consecutive;
}

static Azione decidi_auto(Giocatore *g, int da_pareggiare, int puntata_minima, int *importo_puntato) {
    if (importo_puntato) *importo_puntato = 0;

    RisultatoMano mano;
    int forza = valuta_forza_bot(g, &mano);
    
    int posizione = hero_posizione_iniziale(decidi_tavolo, g->posizione);
    int attivi_dopo = conta_giocatori_attivi_dopo_hero(decidi_tavolo, g->posizione);
    int stack_ratio = (decidi_tavolo->grande_buio > 0) ? g->stack / decidi_tavolo->grande_buio : 50;
    int pot_commitment = (decidi_tavolo->piatto > 0) ? (100 * da_pareggiare) / decidi_tavolo->piatto : 0;
    int draw = ha_draw_forte(g);

    /* Preflop: piu' tight all'early, piu' loose al button/blind */
    if (decidi_tavolo->fase == FASE_PREFLOP) {
        int puo_rilanciar_con_pair = (posizione >= 3 || attivi_dopo == 0);
        
        if (da_pareggiare > 0) {
            if (forza >= TRIS) {
                if (importo_puntato) *importo_puntato = decidi_tavolo->grande_buio * 3;
                return AZIONE_RAISE;
            }
            if (forza == DOPPIA_COPPIA || (forza == COPPIA && puo_rilanciar_con_pair)) {
                if (da_pareggiare > decidi_tavolo->grande_buio * 2 && posizione < 3) {
                    return AZIONE_FOLD;
                }
                if (da_pareggiare <= decidi_tavolo->grande_buio * 3) {
                    if (importo_puntato) *importo_puntato = decidi_tavolo->grande_buio * 2;
                    return AZIONE_RAISE;
                }
                return AZIONE_CALL;
            }
            if (forza == COPPIA || (forza == HIGH_CARD && posizione >= 3)) {
                return (da_pareggiare <= decidi_tavolo->grande_buio) ? AZIONE_CALL : AZIONE_FOLD;
            }
            return AZIONE_FOLD;
        } else {
            if (forza >= COPPIA || (posizione >= 3 && forza >= HIGH_CARD)) {
                if (importo_puntato) *importo_puntato = decidi_tavolo->grande_buio * 2;
                return AZIONE_RAISE;
            }
        }
        return AZIONE_CHECK;
    }

    /* Postflop: valuta draw e posizione */
    int mano_forte = (forza >= TRIS) ||
                     (forza == DOPPIA_COPPIA && decidi_tavolo->n_board >= 3) ||
                     (forza == COPPIA && decidi_tavolo->n_board >= 4);
    
    int mano_media = (forza == COPPIA && decidi_tavolo->n_board <= 3) ||
                     (forza == HIGH_CARD && draw);
    
    if (da_pareggiare > 0) {
        if (mano_forte) {
            int raise_amount = (decidi_tavolo->piatto / 2 > puntata_minima * 2) ? 
                              decidi_tavolo->piatto / 2 : puntata_minima * 2;
            if (importo_puntato) *importo_puntato = raise_amount;
            return AZIONE_RAISE;
        }
        
        if (mano_media && pot_commitment <= 30) {
            if (importo_puntato) *importo_puntato = puntata_minima;
            return AZIONE_RAISE;
        }
        
        if (pot_commitment <= 25 && stack_ratio > 10) {
            return AZIONE_CALL;
        }
        
        if ((forza == COPPIA || (draw && pot_commitment <= 35)) && 
            da_pareggiare <= decidi_tavolo->grande_buio * 2) {
            return AZIONE_CALL;
        }
        
        return AZIONE_FOLD;
    }

    /* Check/bet when no bet faced */
    if (mano_forte) {
        int bet_amount = decidi_tavolo->piatto / 2;
        if (bet_amount < puntata_minima) bet_amount = puntata_minima;
        if (importo_puntato) *importo_puntato = bet_amount;
        return AZIONE_RAISE;
    }
    
    if (mano_media && rand() % 100 < 60) {
        int bet_amount = decidi_tavolo->piatto / 3;
        if (bet_amount < puntata_minima) bet_amount = puntata_minima;
        if (importo_puntato) *importo_puntato = bet_amount;
        return AZIONE_RAISE;
    }

    return AZIONE_CHECK;
}

static void stampa_stack(const Tavolo *t) {
    for (int i = 0; i < t->n_giocatori; i++) {
        printf("  %s: stack=%d stato=%d\n", t->giocatori[i].nome,
               t->giocatori[i].stack, t->giocatori[i].stato);
    }
}

int main(void) {
    const char *nomi[] = {"Alice", "Bob", "Carol"};
    Tavolo t;
    tavolo_init(&t, 3, nomi, 1000, 10, 20);
    decidi_tavolo = &t;

    /* seed RNG for CLI bots */
    srand((unsigned)time(NULL));

    tavolo_nuova_mano(&t);
    printf("=== Nuova mano | dealer: %s ===\n", t.giocatori[t.dealer].nome);
    printf("Piatto dopo i bui: %d\n\n", t.piatto);

    int primo = (t.dealer + 3) % t.n_giocatori; /* dopo big blind */

    printf("--- Preflop ---\n");
    tavolo_giro_puntate(&t, decidi_auto, primo);
    printf("Piatto: %d\n\n", t.piatto);

    tavolo_rivela_flop(&t);
    printf("--- Flop ---\n");
    tavolo_stampa_board(&t);
    tavolo_giro_puntate(&t, decidi_auto, (t.dealer + 1) % t.n_giocatori);
    printf("Piatto: %d\n\n", t.piatto);

    tavolo_rivela_turn(&t);
    printf("--- Turn ---\n");
    tavolo_stampa_board(&t);
    tavolo_giro_puntate(&t, decidi_auto, (t.dealer + 1) % t.n_giocatori);
    printf("Piatto: %d\n\n", t.piatto);

    tavolo_rivela_river(&t);
    printf("--- River ---\n");
    tavolo_stampa_board(&t);
    tavolo_giro_puntate(&t, decidi_auto, (t.dealer + 1) % t.n_giocatori);
    printf("Piatto: %d\n\n", t.piatto);

    printf("--- Showdown ---\n");
    char buf[32];
    for (int i = 0; i < t.n_giocatori; i++) {
        if (t.giocatori[i].stato == ATTIVO || t.giocatori[i].stato == ALL_IN) {
            printf("%s: ", t.giocatori[i].nome);
            for (int c = 0; c < 2; c++) {
                carta_to_string(&t.giocatori[i].mano[c], buf, sizeof(buf));
                printf("[%s] ", buf);
            }
            printf("\n");
        }
    }
    tavolo_assegna_piatto(&t);

    printf("\n=== Stack finali ===\n");
    stampa_stack(&t);

    return 0;
}
