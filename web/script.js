/* script.js
 * Nessuna logica di gioco qui: JS si limita a chiamare le funzioni C
 * esposte dal modulo Wasm e a disegnare lo stato JSON che riceve.
 */

const AZIONE = { FOLD: 0, CHECK: 1, CALL: 2, RAISE: 3 };

let Module;
let creaPartita, nuovaMano, avanza, azioneUmano, statoJson;

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

function disegnaStato(stato) {
  document.getElementById('fase').textContent = stato.fase;
  document.getElementById('piatto').textContent = stato.piatto;
  document.getElementById('board').textContent = stato.board.length
    ? stato.board.join(' ')
    : '(nessuna carta comune)';

  const cont = document.getElementById('giocatori');
  cont.innerHTML = '';
  stato.giocatori.forEach((g, i) => {
    const div = document.createElement('div');
    div.className = 'giocatore' + (i === stato.turno ? ' turno' : '');
    div.innerHTML = `
      <strong>${g.nome}</strong><br>
      Stack: ${g.stack}<br>
      Puntata: ${g.puntata}<br>
      <div class="carte">${g.carte.join(' ')}</div>
    `;
    cont.appendChild(div);
  });

  const controlliUmano = ['btn-fold', 'btn-check', 'btn-call', 'btn-raise', 'raise-importo'];
  const attivaControlli = stato.turno === 0; /* HUMAN_SEAT */
  controlliUmano.forEach(id => {
    document.getElementById(id).disabled = !attivaControlli;
  });
}

function messaggio(testo) {
  document.getElementById('messaggi').textContent = testo;
}

function avanzaEAggiorna() {
  const turno = avanza();
  const stato = leggiStato();
  disegnaStato(stato);

  if (turno === -1) {
    messaggio('Mano conclusa. Premi "Nuova Mano" per continuare.');
  } else {
    messaggio('Tocca a te.');
  }
}

function eseguiAzione(azione, importo = 0) {
  azioneUmano(azione, importo);
  avanzaEAggiorna();
}

window.addEventListener('DOMContentLoaded', () => {
  document.getElementById('btn-nuova-mano').addEventListener('click', () => {
    nuovaMano();
    avanzaEAggiorna();
  });
  document.getElementById('btn-fold').addEventListener('click', () => eseguiAzione(AZIONE.FOLD));
  document.getElementById('btn-check').addEventListener('click', () => eseguiAzione(AZIONE.CHECK));
  document.getElementById('btn-call').addEventListener('click', () => eseguiAzione(AZIONE.CALL));
  document.getElementById('btn-raise').addEventListener('click', () => {
    const importo = parseInt(document.getElementById('raise-importo').value, 10) || 0;
    eseguiAzione(AZIONE.RAISE, importo);
  });

  PokerModule().then(mod => {
    Module = mod;
    collegaFunzioniC();
    creaPartita();
    messaggio('Premi "Nuova Mano" per iniziare.');
  });
});
