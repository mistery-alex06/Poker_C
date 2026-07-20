#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "wasm_bridge.h"
#include "game.h"

typedef enum { FI_ATTESA_MANO = 0, FI_BETTING, FI_FINE_MANO } FaseInterna;

static Tavolo tavolo;
static FaseInterna fase_interna = FI_ATTESA_MANO;

/* Numero massimo di rilanci consentiti in un singolo giro di puntate
 * (bui compresi non contano). Oltre questo tetto i bot possono solo
 * call/check/fold: evita le guerre di rilanci bot-contro-bot che
 * altrimenti potrebbero proseguire finche' lo stack non si esaurisce. */
#define MAX_RILANCI_PER_GIRO 4

static struct {
    int idx;
    int da_agire;
    int bot_ha_rilanciato;
    int n_rilanci;
} giro;

static void giro_inizia(int primo) {
    giro.idx = primo % tavolo.n_giocatori;
    giro.da_agire = 0;
    giro.bot_ha_rilanciato = 0;
    giro.n_rilanci = 0;
    for (int i = 0; i < tavolo.n_giocatori; i++) {
        if (tavolo.giocatori[i].stato == ATTIVO) giro.da_agire++;
    }
}

static int valuta_forza_bot(const Giocatore *g, RisultatoMano *out) {
    Carta carte[7];
    carte[0] = g->mano[0];
    carte[1] = g->mano[1];
    for (int i = 0; i < tavolo.n_board; i++) {
        carte[2 + i] = tavolo.board[i];
    }

    RisultatoMano r = valuta_migliore_mano(carte, 2 + tavolo.n_board);
    if (out) *out = r;
    return (int)r.tipo;
}

/* Bot aggressivo: decisione ponderata, raise casuale, bluff sul piatto vuoto.
 * Non devono seguire automaticamente il giocatore umano, e non devono
 * fare solo check. */
static Azione bot_decidi(const Giocatore *g, int da_pareggiare, int puntata_minima, int *importo_puntato) {
    if (importo_puntato) *importo_puntato = 0;

    RisultatoMano mano;
    int forza = valuta_forza_bot(g, &mano);

    /* Preferenza raise basata sulla forza della mano. */
    int raise_base;
    int call_base;
    switch (forza) {
        case SCALA_COLORE: case POKER:
            raise_base = 85; call_base = 10; break;
        case FULL_HOUSE:
            raise_base = 75; call_base = 20; break;
        case COLORE:
            raise_base = 70; call_base = 25; break;
        case TRIS:
            raise_base = 65; call_base = 30; break;
        case DOPPIA_COPPIA:
            raise_base = 45; call_base = 45; break;
        case COPPIA:
            raise_base = 35; call_base = 50; break;
        default: /* HIGH_CARD */
            raise_base = 20; call_base = 55; break;
    }

    int r = rand() % 100;
    /* Sizing moderato: il rilancio resta un multiplo contenuto del grande
     * buio (2-4x), con un tetto a un quarto dello stack per evitare che il
     * bot rischi tutto casualmente su ogni mano. */
    int min_raise = puntata_minima > 0 ? puntata_minima : tavolo.grande_buio;
    int max_raise = tavolo.grande_buio * 4;
    int tetto_stack = g->stack / 4;
    if (max_raise > tetto_stack) max_raise = tetto_stack;
    if (max_raise < min_raise) max_raise = min_raise;
    int raise_amount = min_raise;
    if (max_raise > min_raise) {
        raise_amount = min_raise + (rand() % (max_raise - min_raise + 1));
    }
    if (raise_amount > g->stack) raise_amount = g->stack;

    int puo_rilanciare = giro.n_rilanci < MAX_RILANCI_PER_GIRO;
    int bluff_chance = 20;
    int pot_vuoto_raff = (tavolo.piatto == 0);

    if (da_pareggiare > 0) {
        /* Non ci sono check automatici qui. Se non c'è ancora stato un raise
         * nel round e siamo il primo bot che può agire, rilanciamo subito
         * (a meno che il tetto rilanci non sia gia' stato raggiunto). */
        if (puo_rilanciare && !giro.bot_ha_rilanciato) {
            if (importo_puntato) *importo_puntato = raise_amount;
            return AZIONE_RAISE;
        }

        if (puo_rilanciare && r < raise_base + 15) {
            if (importo_puntato) *importo_puntato = raise_amount;
            return AZIONE_RAISE;
        }
        if (r < raise_base + call_base + 10) {
            return AZIONE_CALL;
        }
        return AZIONE_FOLD;
    }

    /* Se il piatto è vuoto, possono bluffare rilanciando anche con mano media. */
    if (puo_rilanciare && !giro.bot_ha_rilanciato) {
        /* Se nessun bot ha ancora rilanciato in questo round, il primo bot
         * ad agire qui deve rilanciare anche su piatto libero. */
        if (importo_puntato) *importo_puntato = raise_amount;
        return AZIONE_RAISE;
    }

    if (puo_rilanciare && ((pot_vuoto_raff && r < bluff_chance) || r < raise_base)) {
        if (importo_puntato) *importo_puntato = raise_amount;
        return AZIONE_RAISE;
    }

    return AZIONE_CHECK;
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
            /* "Raise By": l'incremento si somma alla puntata MASSIMA del
             * tavolo (non alla puntata gia' fatta dal giocatore): la
             * puntata totale a cui il giocatore deve arrivare e'
             * puntata_massima + incremento. Il giocatore mette quindi la
             * differenza rispetto a quanto ha gia' messo in questo giro
             * (necessario = punta_a - puntata_corrente), che copre sia
             * l'eventuale call implicito sia il rilancio vero e proprio.
             * Regola del rilancio minimo: l'incremento deve essere almeno
             * pari al grande buio. */
            int incremento_minimo = tavolo.grande_buio;
            int incremento = (importo > incremento_minimo) ? importo : incremento_minimo;
            int punta_a = tavolo.puntata_massima + incremento;
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
        giro.n_rilanci++;
    }
    if (era_rilancio && seat != HUMAN_SEAT) {
        giro.bot_ha_rilanciato = 1;
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
    const char *nomi[] = {"Tu", "Bot 1", "Bot 2", "Bot 3", "Bot 4"};
    tavolo_init(&tavolo, 5, nomi, 1000, 10, 20);
    fase_interna = FI_ATTESA_MANO;
    /* seed RNG for bot randomness */
    srand((unsigned)time(NULL));
}

/* Numero di giocatori che hanno ancora chip in gioco (torneo non ancora
 * concluso se sono >= 2). */
static int conta_giocatori_con_chip(void) {
    int n = 0;
    for (int i = 0; i < tavolo.n_giocatori; i++) {
        if (tavolo.giocatori[i].stack > 0) n++;
    }
    return n;
}

void wasm_nuova_mano(void) {
    if (conta_giocatori_con_chip() <= 1) return; /* torneo gia' concluso */
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
            return -2; /* fase avanzata: un passo atomico a se', JS mostra il nuovo board e richiama */
        }

        Giocatore *g = &tavolo.giocatori[giro.idx];
        if (g->stato != ATTIVO) {
            giro.idx = (giro.idx + 1) % tavolo.n_giocatori;
            continue;
        }

        if (giro.idx == HUMAN_SEAT) {
            return HUMAN_SEAT; /* aspetta azione da JS */
        }

        int seat_bot = giro.idx;
        int importo_bot = 0;
        Azione azione_bot = bot_decidi(g, tavolo.puntata_massima - g->puntata_corrente,
                                      tavolo.grande_buio, &importo_bot);
        applica_azione(seat_bot, azione_bot, importo_bot);
        return seat_bot; /* un bot per chiamata: JS scandisce il ritmo con un ritardo */
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

    int attivi_torneo = conta_giocatori_con_chip();
    int vincitore_torneo = -1;
    if (attivi_torneo == 1) {
        for (int i = 0; i < tavolo.n_giocatori; i++) {
            if (tavolo.giocatori[i].stack > 0) { vincitore_torneo = i; break; }
        }
    }

    off += snprintf(buf + off, sizeof(buf) - off,
        "{\"fase\":\"%s\",\"piatto\":%d,\"punta_max\":%d,\"dealer\":%d,\"turno\":%d,"
        "\"da_agire\":%d,\"sb\":%d,\"bb\":%d,\"attivi_torneo\":%d,\"vincitore_torneo\":%d,\"board\":[",
        fase_to_string(tavolo.fase), tavolo.piatto, tavolo.puntata_massima, tavolo.dealer,
        (fase_interna == FI_BETTING) ? giro.idx : -1,
        (fase_interna == FI_BETTING) ? giro.da_agire : 0,
        tavolo.piccolo_buio, tavolo.grande_buio, attivi_torneo, vincitore_torneo);

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

    off += snprintf(buf + off, sizeof(buf) - off, "],\"showdown\":");
    if (tavolo.fase == FASE_SHOWDOWN && tavolo.showdown_vincitore >= 0) {
        const char *nome_vincitore = tavolo.giocatori[tavolo.showdown_vincitore].nome;
        const char *combinazione = tipo_mano_to_string(tavolo.showdown_tipo);
        off += snprintf(buf + off, sizeof(buf) - off,
            "{\"vincitore\":%d,\"nome\":\"%s\",\"combinazione\":\"%s\",\"pareggio\":%d}",
            tavolo.showdown_vincitore, nome_vincitore, combinazione,
            tavolo.showdown_n_vincitori > 1);
    } else {
        off += snprintf(buf + off, sizeof(buf) - off, "null");
    }

    off += snprintf(buf + off, sizeof(buf) - off, "}");
    return buf;
}
