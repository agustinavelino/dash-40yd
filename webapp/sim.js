// Hub simulado para desarrollar la pagina sin placas. Implementa PROTOCOLO.md
// en el navegador: index.html lo carga solo con ?sim=1 y NUNCA se embebe en
// el ESP32.
//
// Opciones en la URL:
//   ?sim=1             hub simulado
//   &caida=20          corta el WebSocket cada 20 s durante 3 s (prueba de reconexion)
//   &rapido=1          la corrida llega 1 s despues de armar (en vez de 3-6 s)
// Desde la consola: sim.caer(), sim.corrida(), sim.puerta(2, false)
(function () {
  var q = new URLSearchParams(location.search);
  var CAIDA_S = +q.get("caida") || 0;
  var RAPIDO = q.get("rapido") === "1";

  // --- "flash" del hub: roster y configuracion sobreviven a recargar la pagina ---
  function leer(k, def) { try { return JSON.parse(localStorage.getItem("sim." + k)) || def; } catch (e) { return def; } }
  function guardar(k, v) { try { localStorage.setItem("sim." + k, JSON.stringify(v)); } catch (e) {} }
  var roster = leer("roster", [
    { id: 1, nombre: "Diego Ramírez", num: 22 }, { id: 2, nombre: "Mateo Salinas", num: 7 },
    { id: 3, nombre: "Iker Fuentes", num: 31 }, { id: 4, nombre: "Santiago Ortega", num: 4 },
    { id: 5, nombre: "Emiliano Cruz", num: 88 }]);
  var config = leer("config", { yd: [10, 20, 40] });

  // --- estado del hub (se pierde al "reiniciar", como la RAM del ESP32) ---
  var hub = { armado: false, atleta: null, hora: null, inicio: null, corridas: [], n: 0,
              puertaOk: [true, true, true, true], resync: [0, 0, 0, 0] };
  var conectado = null, timerCorrida = null;

  function estado() {
    var yd = [0].concat(config.yd);
    return { t: "estado", armado: hub.armado, atleta: hub.atleta,
             puertas: yd.map(function (d, i) {
               return { i: i, yd: d, ok: hub.puertaOk[i], resync_s: hub.puertaOk[i] ? hub.resync[i] : null };
             }) };
  }

  // Splits de un juvenil: 40 yd entre 4.8 y 6.2 s. Las fracciones 10/20 yd salen
  // del modelo mono-exponencial (Maestro 14.2) con algo de ruido por atleta.
  function splits() {
    var t40 = 4800 + Math.random() * 1400;
    var f10 = 0.335 + Math.random() * 0.03, f20 = 0.565 + Math.random() * 0.03;
    var s = [Math.round(t40 * f10), Math.round(t40 * f20), Math.round(t40)];
    for (var i = 0; i < 3; i++) if (!hub.puertaOk[i + 1] || Math.random() < 0.05) s[i] = null;
    return s;
  }

  function cerrarCorrida() {
    timerCorrida = null;
    if (!hub.armado) return;
    var c = { id: ++hub.n, atleta: hub.atleta, splits_ms: splits(), yd: config.yd.slice(),
              ts: hub.hora ? Math.round(hub.hora + (performance.now() - hub.horaRef) / 1000) : null };
    hub.corridas.push(c);
    hub.armado = false;                       // tras una corrida el hub queda desarmado
    window.sim.ultimoEnvio = performance.now();
    enviar(Object.assign({ t: "corrida" }, c));
    enviar(estado());
  }

  function enviar(o) { if (conectado) conectado._recibir(JSON.stringify(o)); }

  function comando(m) {
    if (m.c === "hora") { hub.hora = m.epoch; hub.horaRef = performance.now(); if (!hub.inicio) hub.inicio = m.epoch; }
    else if (m.c === "armar") {
      if (!roster.some(function (a) { return a.id === m.atleta; })) return enviar({ t: "error", msg: "no se puede armar sin atleta" });
      hub.armado = true; hub.atleta = m.atleta;
      clearTimeout(timerCorrida);
      timerCorrida = setTimeout(cerrarCorrida, RAPIDO ? 1000 : 3000 + Math.random() * 3000);
      enviar(estado());
    } else if (m.c === "desarmar") { hub.armado = false; clearTimeout(timerCorrida); enviar(estado()); }
  }

  // --- WebSocket falso con la misma interfaz que el real ---
  function SimWS() {
    var ws = this;
    ws.readyState = 0;
    setTimeout(function () {
      if (caido) { ws.readyState = 3; if (ws.onclose) ws.onclose({}); return; }
      ws.readyState = 1; conectado = ws;
      if (ws.onopen) ws.onopen({});
      enviar(estado());
    }, 150);
  }
  SimWS.prototype.send = function (txt) {
    if (this.readyState !== 1) throw new Error("WebSocket no abierto");
    var m = JSON.parse(txt);
    setTimeout(function () { comando(m); }, 30);
  };
  SimWS.prototype.close = function () {
    if (this.readyState === 3) return;
    this.readyState = 3; if (conectado === this) conectado = null;
    if (this.onclose) this.onclose({});
  };
  SimWS.prototype._recibir = function (txt) { if (this.onmessage) this.onmessage({ data: txt }); };

  var caido = false;
  function caer(ms) {
    caido = true;
    if (conectado) conectado.close();
    setTimeout(function () { caido = false; }, ms || 3000);
  }

  // latido y resync que avanza
  setInterval(function () {
    for (var i = 0; i < 4; i++) hub.resync[i] = i === 3 ? 0 : (hub.resync[i] + 5) % 30;
    enviar(estado());
  }, 5000);
  if (CAIDA_S) setInterval(caer, CAIDA_S * 1000);

  // --- HTTP falso ---
  function resp(cuerpo, codigo) {
    return Promise.resolve({ ok: !codigo, status: codigo || 200,
                             json: function () { return Promise.resolve(JSON.parse(JSON.stringify(cuerpo))); } });
  }
  function simFetch(ruta, opc) {
    var metodo = (opc && opc.method) || "GET";
    var cuerpo = opc && opc.body ? JSON.parse(opc.body) : null;
    return new Promise(function (ok) { setTimeout(ok, 80); }).then(function () {
      if (ruta === "/api/roster" && metodo === "GET") return resp(roster);
      if (ruta === "/api/roster" && metodo === "POST") {
        var max = roster.reduce(function (m, a) { return Math.max(m, a.id); }, 0);
        roster = cuerpo.map(function (a) { return { id: a.id == null ? ++max : a.id, nombre: a.nombre, num: a.num }; });
        guardar("roster", roster); return resp(roster);
      }
      if (ruta === "/api/config" && metodo === "GET") return resp(config);
      if (ruta === "/api/config" && metodo === "POST") {
        var y = cuerpo.yd;
        if (!(y.length === 3 && y[0] > 0 && y[0] < y[1] && y[1] < y[2])) return resp({ msg: "distancias no crecientes" }, 400);
        config = { yd: y }; guardar("config", config); enviar(estado()); return resp(config);
      }
      if (ruta === "/api/sesion") return resp({ inicio: hub.inicio, corridas: hub.corridas });
      return resp({ msg: "no existe" }, 404);
    });
  }

  window.DASH_WS = SimWS;
  window.DASH_FETCH = simFetch;
  window.sim = {
    caer: caer,
    corrida: function () { if (!hub.armado) { hub.armado = true; hub.atleta = hub.atleta || roster[0].id; } cerrarCorrida(); },
    puerta: function (i, ok) { hub.puertaOk[i] = ok; enviar(estado()); },
    ultimoEnvio: null
  };
})();
