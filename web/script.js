/* script.js
 * Nessuna logica di gioco qui: JS si limita a chiamare le funzioni C
 * esposte dal modulo Wasm e a disegnare lo stato JSON che riceve.
 */

const AZIONE = { FOLD: 0, CHECK: 1, CALL: 2, RAISE: 3 };
const STATO = { ATTIVO: 0, FOLD: 1, ALL_IN: 2, ELIMINATO: 3 };
const HUMAN_SEAT = 0;

/* Ritardi pensati per sembrare "umani": ogni bot pensa un tempo casuale
 * diverso, il cambio fase (flop/turn/river) ha una pausa piu' breve. */
const RITARDO_BOT_MIN_MS = 1400;
const RITARDO_BOT_MAX_MS = 2600;
const RITARDO_FASE_MS = 1300;

/* Mappa i codici carta del motore C (es. "10F", "AQ") in simbolo + colore.
 * Semi: Q=Quadri(♦) C=Cuori(♥) F=Fiori(♣) P=Picche(♠) */
const SUITS = {
  Q: { simbolo: '♦', colore: 'red' },
  C: { simbolo: '♥', colore: 'red' },
  F: { simbolo: '♣', colore: 'black' },
  P: { simbolo: '♠', colore: 'black' },
};

let Module;
let creaPartita, nuovaMano, avanza, azioneUmano, statoJson;
const registro = [];

function collegaFunzioniC() {
  creaPartita = Module.cwrap('wasm_crea_partita', null, []);
  nuovaMano   = Module.cwrap('wasm_nuova_mano', null, []);
  avanza      = Module.cwrap('wasm_avanza', 'number', []);
  azioneUmano = Module.cwrap('wasm_azione_umano', null, ['number', 'number']);
  statoJson   = Module.cwrap('wasm_stato_json', 'string', []);
}

function leggiStato() {
  return JSON.parse(statoJson());
}

function ritardoBotCasuale() {
  return RITARDO_BOT_MIN_MS + Math.random() * (RITARDO_BOT_MAX_MS - RITARDO_BOT_MIN_MS);
}

/* ---------- Rendering carte / seat ---------- */

function creaCartaEl(codice, coperta) {
  const el = document.createElement('div');
  if (coperta || codice === '??') {
    el.className = 'card back';
    return el;
  }
  const seme = codice.slice(-1);
  const valore = codice.slice(0, -1);
  const info = SUITS[seme] || { simbolo: '?', colore: 'black' };
  el.className = `card ${info.colore}`;
  el.innerHTML = `<span>${valore}</span><span class="suit">${info.simbolo}</span>`;
  return el;
}

function riempiCarte(contenitoreId, carte, coperta) {
  const cont = document.getElementById(contenitoreId);
  cont.innerHTML = '';
  carte.forEach(c => cont.appendChild(creaCartaEl(c, coperta)));
}

function aggiornaSeat(prefix, giocatore, seatEl, inTurno) {
  document.getElementById(`${prefix}-nome`).textContent = giocatore.nome;
  document.getElementById(`${prefix}-stack`).textContent = giocatore.stack;
  document.getElementById(`${prefix}-puntata`).textContent = giocatore.puntata;

  const coperta = prefix !== 'human' && giocatore.carte[0] === '??';
  riempiCarte(`${prefix}-carte`, giocatore.carte, coperta);

  seatEl.classList.toggle('turno', inTurno);
  seatEl.classList.toggle('fold', giocatore.stato === STATO.FOLD);
  seatEl.classList.toggle('eliminato', giocatore.stato === STATO.ELIMINATO);
}

function disegnaStato(stato) {
  document.getElementById('fase').textContent = stato.fase;
  document.getElementById('piatto').textContent = stato.piatto;

  riempiCarte('board', stato.board, false);

  const g = stato.giocatori; /* g[0]=Tu, g[1]=Bot1, g[2]=Bot2 (HUMAN_SEAT=0) */
  aggiornaSeat('human', g[0], document.getElementById('seat-human'), stato.turno === 0);
  aggiornaSeat('bot1', g[1], document.getElementById('seat-bot1'), stato.turno === 1);
  aggiornaSeat('bot2', g[2], document.getElementById('seat-bot2'), stato.turno === 2);

  document.getElementById('dealer-nome').textContent = g[stato.dealer].nome;

  const inTurnoUmano = stato.turno === HUMAN_SEAT;
  const daPareggiare = inTurnoUmano ? (stato.punta_max - g[HUMAN_SEAT].puntata) : 0;

  /* Regole di validazione azioni (Texas Hold'em):
   * - puntata > 0 (c'e' da pareggiare) -> disponibili solo Call, Raise, Fold
   * - puntata == 0 (nessuna puntata da pareggiare) -> disponibile Check, non Call
   * Check non deve MAI essere cliccabile quando c'e' una puntata attiva:
   * il motore C lo tratterebbe come fold difensivo, un esito che l'utente
   * non ha scelto esplicitamente. */
  document.getElementById('btn-check').disabled = !inTurnoUmano || daPareggiare > 0;
  document.getElementById('btn-call').disabled  = !inTurnoUmano || daPareggiare <= 0;
  document.getElementById('btn-fold').disabled  = !inTurnoUmano;
  document.getElementById('btn-raise').disabled = !inTurnoUmano;
  document.getElementById('raise-importo').disabled = !inTurnoUmano;
}

function messaggio(testo) {
  document.getElementById('messaggi').textContent = testo;
}

/* ---------- Log azioni ---------- */

function aggiungiLog(testo, isDivisore = false) {
  registro.push({ testo, isDivisore });
  const cont = document.getElementById('log-azioni');
  const riga = document.createElement('div');
  riga.className = 'log-riga' + (isDivisore ? ' log-divisore' : '');
  riga.textContent = testo;
  cont.appendChild(riga);
  cont.scrollTop = cont.scrollHeight;
}

function pulisciLog() {
  registro.length = 0;
  document.getElementById('log-azioni').innerHTML = '';
}

/* Descrive cosa ha fatto seatIdx confrontando lo stato prima/dopo la sua
 * mossa. Funziona sia per i bot che per l'azione dell'umano. */
function descriviAzione(seatIdx, prima, dopo) {
  const nome = prima.giocatori[seatIdx].nome;
  const pGiocatore = prima.giocatori[seatIdx];
  const dGiocatore = dopo.giocatori[seatIdx];

  if (dGiocatore.stato === STATO.FOLD && pGiocatore.stato !== STATO.FOLD) {
    return `${nome} si ritira (fold)`;
  }

  const delta = dGiocatore.puntata - pGiocatore.puntata;

  if (delta === 0) {
    return `${nome} passa (check)`;
  }
  if (dGiocatore.stato === STATO.ALL_IN && pGiocatore.stato !== STATO.ALL_IN) {
    return `${nome} va all-in con ${dGiocatore.puntata}`;
  }
  if (dopo.punta_max > prima.punta_max) {
    const raiseBy = Math.max(0, dGiocatore.puntata - pGiocatore.puntata);
    return `${nome} rilancia di ${raiseBy}`;
  }
  return `${nome} paga ${delta}`;
}

/* ---------- Motore dei turni: peek -> pausa -> risolvi -> log -> ripeti ---------- */

/* Guarda lo stato attuale e decide cosa fare, SENZA ancora chiedere al
 * motore C di risolvere nulla: se tocca a un bot con un'azione pendente
 * reale (da_agire>0), mostra "sta decidendo..." e aspetta un tempo
 * casuale prima di procedere, cosi' i turni sembrano scanditi da una
 * vera riflessione invece che istantanei. */
function passo() {
  const stato = leggiStato();

  if (stato.turno === -1) {
    return;
  }
  if (stato.turno === HUMAN_SEAT) {
    messaggio('Tocca a te.');
    return;
  }
  if (stato.da_agire <= 0) {
    /* la prossima chiamata a avanza() fara' scattare un cambio fase, non
     * un'azione di un giocatore: nessuna attesa "sta pensando" qui. */
    risolviUnPasso();
    return;
  }

  const nomeBot = stato.giocatori[stato.turno].nome;
  messaggio(`${nomeBot} sta decidendo...`);
  setTimeout(risolviUnPasso, ritardoBotCasuale());
}

/* Chiede al motore C di risolvere ESATTAMENTE un passo atomico (una
 * transizione di fase, oppure l'azione di un singolo giocatore) e
 * aggiorna log/UI di conseguenza. */
function risolviUnPasso() {
  const prima = leggiStato();
  const risultato = avanza();
  const dopo = leggiStato();
  disegnaStato(dopo);

  if (risultato === -1) {
    if (dopo.showdown && dopo.showdown.nome) {
      const messaggioShowdown = dopo.showdown.pareggio
        ? `${dopo.showdown.nome} condivide il piatto con ${dopo.showdown.combinazione}`
        : `${dopo.showdown.nome} vince con ${dopo.showdown.combinazione}`;
      aggiungiLog(messaggioShowdown, true);
    }
    aggiungiLog('— Fine mano —', true);
    messaggio('Mano conclusa. Premi "Nuova Mano" per continuare.');
    return;
  }
  if (risultato === -2) {
    aggiungiLog(`— ${dopo.fase} —`, true);
    setTimeout(passo, RITARDO_FASE_MS);
    return;
  }
  if (risultato === HUMAN_SEAT) {
    messaggio('Tocca a te.');
    return;
  }

  aggiungiLog(descriviAzione(risultato, prima, dopo));
  passo();
}

function eseguiAzione(azione, importo = 0) {
  const prima = leggiStato();
  azioneUmano(azione, importo);
  const dopo = leggiStato();
  disegnaStato(dopo);
  aggiungiLog(descriviAzione(HUMAN_SEAT, prima, dopo));
  passo();
}

window.addEventListener('DOMContentLoaded', () => {
  document.getElementById('btn-nuova-mano').addEventListener('click', () => {
    pulisciLog();
    nuovaMano();
    const stato = leggiStato();
    disegnaStato(stato);

    aggiungiLog('— Nuova mano —', true);
    const sbIdx = (stato.dealer + 1) % stato.giocatori.length;
    const bbIdx = (stato.dealer + 2) % stato.giocatori.length;
    aggiungiLog(`${stato.giocatori[sbIdx].nome} posiziona il piccolo buio (${stato.sb})`);
    aggiungiLog(`${stato.giocatori[bbIdx].nome} posiziona il grande buio (${stato.bb})`);

    passo();
  });
  function handleRaise() {
    const importo = parseInt(document.getElementById('raise-importo').value, 10) || 0;
    eseguiAzione(AZIONE.RAISE, importo);
  }

  document.getElementById('btn-fold').addEventListener('click', () => eseguiAzione(AZIONE.FOLD));
  document.getElementById('btn-check').addEventListener('click', () => eseguiAzione(AZIONE.CHECK));
  document.getElementById('btn-call').addEventListener('click', () => eseguiAzione(AZIONE.CALL));
  document.getElementById('btn-raise').addEventListener('click', handleRaise);

  PokerModule().then(mod => {
    Module = mod;
    collegaFunzioniC();
    creaPartita();
    messaggio('Premi "Nuova Mano" per iniziare.');
  });
});
