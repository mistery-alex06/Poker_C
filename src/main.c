#include <stdio.h>
#include "game.h"

/* Bot semplicissimo: check se gratis, altrimenti call. Mai fold, mai raise. */
static Azione decidi_auto(Giocatore *g, int da_pareggiare, int puntata_minima, int *importo_puntato) {
    (void)g; (void)puntata_minima; (void)importo_puntato;
    return (da_pareggiare > 0) ? AZIONE_CALL : AZIONE_CHECK;
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
