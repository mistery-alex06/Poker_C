#ifndef PLAYER_H
#define PLAYER_H

#include "deck.h"

#define NOME_MAX_LEN 32
#define CARTE_IN_MANO 2

typedef enum {
    ATTIVO = 0,   /* in gioco, puo' agire */
    FOLD,         /* si e' ritirato dalla mano corrente */
    ALL_IN,       /* ha puntato tutto lo stack */
    ELIMINATO     /* stack a zero, fuori dal torneo */
} StatoGiocatore;

typedef struct {
    char nome[NOME_MAX_LEN];
    Carta mano[CARTE_IN_MANO];
    int stack;              /* chip disponibili */
    int puntata_corrente;   /* quanto ha messo nel giro di puntate attuale */
    int puntata_totale_mano; /* quanto ha messo nell'intera mano (per side pot) */
    StatoGiocatore stato;
    int posizione;          /* seat index al tavolo */
} Giocatore;

void player_init(Giocatore *g, const char *nome, int stack_iniziale, int posizione);
void player_reset_mano(Giocatore *g);          /* nuova mano: pulisce carte/puntate, riattiva se non eliminato */
void player_assegna_carta(Giocatore *g, int idx, Carta c);
int  player_punta(Giocatore *g, int importo);   /* ritorna importo realmente puntato (cap allo stack) */
void player_fold(Giocatore *g);
int  player_e_attivo(const Giocatore *g);       /* ATTIVO o ALL_IN: puo' vincere il piatto */

#endif /* PLAYER_H */
