#include <stdio.h>
#include <string.h>
#include "wasm_bridge.h"
#include "game.h"

typedef enum { FI_ATTESA_MANO = 0, FI_BETTING, FI_FINE_MANO } FaseInterna;

static Tavolo tavolo;
static FaseInterna fase_interna = FI_ATTESA_MANO;

static struct {
    int idx;
    int da_agire;
} giro;

static void giro_inizia(int primo) {
    giro.idx = primo % tavolo.n_giocatori;
    giro.da_agire = 0;
    for (int i = 0; i < tavolo.n_giocatori; i++) {
        if (tavolo.giocatori[i].stato == ATTIVO) giro.da_agire++;
    }
}

/* Bot elementare: call se c'e' da pareggiare, altrimenti check. Mai fold/raise. */
static Azione bot_decidi(const Giocatore *g) {
    return (tavolo.puntata_massima - g->puntata_corrente > 0) ? AZIONE_CALL : AZIONE_CHECK;
}

static void applica_azione(int seat, Azione a, int importo) {
    Giocatore *g = &tavolo.giocatori[seat];
    int da_pareggiare = tavolo.puntata_massima - g->puntata_corrente;
    int era_rilancio = 0;

    switch (a) {
        case AZIONE_FOLD:
            player_fold(g);
            break;
        case AZIONE_CHECK:
            if (da_pareggiare > 0) player_fold(g);
            break;
        case AZIONE_CALL:
            tavolo.piatto += player_punta(g, da_pareggiare);
            break;
        case AZIONE_RAISE: {
            int punta_minima = tavolo.puntata_massima + tavolo.grande_buio;
            int punta_a = (importo > punta_minima) ? importo : punta_minima;
            int necessario = punta_a - g->puntata_corrente;
            tavolo.piatto += player_punta(g, necessario);
            if (g->puntata_corrente > tavolo.puntata_massima) {
                tavolo.puntata_massima = g->puntata_corrente;
                era_rilancio = 1;
            }
            break;
        }
    }

    if (era_rilancio) {
        int altri = 0;
        for (int i = 0; i < tavolo.n_giocatori; i++) {
            if (i != seat && tavolo.giocatori[i].stato == ATTIVO) altri++;
        }
        giro.da_agire = altri;
    } else {
        giro.da_agire--;
    }

    giro.idx = (seat + 1) % tavolo.n_giocatori;
    if (tavolo_conta_attivi_in_mano(&tavolo) <= 1) giro.da_agire = 0;
}

void wasm_crea_partita(void) {
    const char *nomi[] = {"Tu", "Bot 1", "Bot 2"};
    tavolo_init(&tavolo, 3, nomi, 1000, 10, 20);
    fase_interna = FI_ATTESA_MANO;
}

void wasm_nuova_mano(void) {
    tavolo_nuova_mano(&tavolo);
    fase_interna = FI_BETTING;
    giro_inizia((tavolo.dealer + 3) % tavolo.n_giocatori); /* dopo il grande buio */
}

int wasm_avanza(void) {
    if (fase_interna == FI_FINE_MANO) return -1;

    int guardia = 0;
    int guardia_max = tavolo.n_giocatori * 100;

    while (guardia++ < guardia_max) {
        if (giro.da_agire <= 0 || tavolo_conta_attivi_in_mano(&tavolo) <= 1) {
            if (tavolo.fase == FASE_RIVER || tavolo_conta_attivi_in_mano(&tavolo) <= 1) {
                tavolo_assegna_piatto(&tavolo);
                fase_interna = FI_FINE_MANO;
                return -1;
            }
            if (tavolo.fase == FASE_PREFLOP) {
                tavolo_rivela_flop(&tavolo);
            } else if (tavolo.fase == FASE_FLOP) {
                tavolo_rivela_turn(&tavolo);
            } else if (tavolo.fase == FASE_TURN) {
                tavolo_rivela_river(&tavolo);
            }
            giro_inizia((tavolo.dealer + 1) % tavolo.n_giocatori);
            continue;
        }

        Giocatore *g = &tavolo.giocatori[giro.idx];
        if (g->stato != ATTIVO) {
            giro.idx = (giro.idx + 1) % tavolo.n_giocatori;
            continue;
        }

        if (giro.idx == HUMAN_SEAT) {
            return HUMAN_SEAT; /* aspetta azione da JS */
        }

        applica_azione(giro.idx, bot_decidi(g), 0);
    }

    /* guardia di sicurezza: non dovrebbe mai arrivare qui */
    fase_interna = FI_FINE_MANO;
    return -1;
}

void wasm_azione_umano(int azione, int importo) {
    if (fase_interna != FI_BETTING) return;
    if (giro.idx != HUMAN_SEAT) return;
    applica_azione(HUMAN_SEAT, (Azione)azione, importo);
}

static void carta_codice(Carta c, char *out, size_t n) {
    static const char semi[] = {'Q', 'C', 'F', 'P'}; /* Quadri Cuori Fiori Picche */
    snprintf(out, n, "%s%c", valore_to_string(c.valore), semi[c.seme]);
}

static const char *fase_to_string(FaseGioco f) {
    switch (f) {
        case FASE_PREFLOP:  return "PREFLOP";
        case FASE_FLOP:     return "FLOP";
        case FASE_TURN:     return "TURN";
        case FASE_RIVER:    return "RIVER";
        case FASE_SHOWDOWN: return "SHOWDOWN";
        default:            return "?";
    }
}

const char *wasm_stato_json(void) {
    static char buf[4096];
    int off = 0;
    char cod[8];
    int mostra_carte = (fase_interna == FI_FINE_MANO);

    off += snprintf(buf + off, sizeof(buf) - off,
        "{\"fase\":\"%s\",\"piatto\":%d,\"punta_max\":%d,\"dealer\":%d,\"turno\":%d,\"board\":[",
        fase_to_string(tavolo.fase), tavolo.piatto, tavolo.puntata_massima, tavolo.dealer,
        (fase_interna == FI_BETTING) ? giro.idx : -1);

    for (int i = 0; i < tavolo.n_board; i++) {
        carta_codice(tavolo.board[i], cod, sizeof(cod));
        off += snprintf(buf + off, sizeof(buf) - off, "%s\"%s\"", i ? "," : "", cod);
    }
    off += snprintf(buf + off, sizeof(buf) - off, "],\"giocatori\":[");

    for (int i = 0; i < tavolo.n_giocatori; i++) {
        Giocatore *g = &tavolo.giocatori[i];
        int rivela = mostra_carte || (i == HUMAN_SEAT);
        char c0[8] = "??", c1[8] = "??";
        if (rivela) {
            carta_codice(g->mano[0], c0, sizeof(c0));
            carta_codice(g->mano[1], c1, sizeof(c1));
        }
        off += snprintf(buf + off, sizeof(buf) - off,
            "%s{\"nome\":\"%s\",\"stack\":%d,\"puntata\":%d,\"stato\":%d,\"carte\":[\"%s\",\"%s\"]}",
            i ? "," : "", g->nome, g->stack, g->puntata_corrente, g->stato, c0, c1);
    }

    off += snprintf(buf + off, sizeof(buf) - off, "]}");
    return buf;
}
