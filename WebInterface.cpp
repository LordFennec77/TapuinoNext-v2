#include "WebInterface.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Update.h>
#include "SD_MMC.h"
#include "config.h"

namespace {
constexpr const char* AP_SSID = "TapuinoNext";
// Password WPA2 >= 8 caratteri; usa "" per creare una rete AP aperta
constexpr const char* AP_PASS = "12345678";
constexpr const char* PREF_NAMESPACE = "tapuino-wifi";
}

WebServer server(80);
Preferences wifiPrefs;

volatile bool remotePlayRequested = false;
volatile bool remoteStopRequested = false;
volatile bool isTapPlaying = false;
volatile bool c64MotorActive = false;
volatile uint16_t tapCounterCurrent = 0;
std::string remoteFilePath = "";
static bool otaUploadOk = false;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>TapuinoNext Web Control</title>
  <style>
    :root, body.theme-c64 {
      --bg: #6c5eb5; --card: #352879; --sub: #2b1f66; --inp: #221754;
      --fg: #a39bff; --acc: #7c70da; --btn: #7c70da; --btntxt: #1a1240; --ok: #55ff55;
      --borderw: 10px;
    }
    body.theme-dark {
      --bg: #0b0d12; --card: #161a24; --sub: #10141c; --inp: #080a0f;
      --fg: #e6ebff; --acc: #4ea1ff; --btn: #4ea1ff; --btntxt: #08111e; --ok: #4ade80;
      --borderw: 4px;
    }
    body.theme-amiga {
      --bg: #0055aa; --card: #0a2e5c; --sub: #072245; --inp: #041730;
      --fg: #ffaa00; --acc: #ffffff; --btn: #ffaa00; --btntxt: #041730; --ok: #66ff99;
      --borderw: 6px;
    }
    body.theme-phosphor {
      --bg: #07130a; --card: #0c2112; --sub: #08180d; --inp: #051109;
      --fg: #66ff88; --acc: #22cc55; --btn: #33dd66; --btntxt: #051109; --ok: #aaffaa;
      --borderw: 4px;
    }
    body.theme-amber {
      --bg: #170e04; --card: #261707; --sub: #1c1004; --inp: #120a02;
      --fg: #ffb84d; --acc: #e68a00; --btn: #ff9f1a; --btntxt: #170e04; --ok: #ffd166;
      --borderw: 4px;
    }
    body.lg { font-size: 16px; }
    body { background: var(--bg); color: var(--fg); font-family: monospace; text-align: center; margin: 0; padding: 12px; font-size: 14px; }
    .container {
      max-width: 740px; margin: 0 auto; background: var(--card); padding: 14px;
      border: var(--borderw) solid var(--acc); box-shadow: 0 0 0 2px #1a1240;
    }
    .topbar { display: flex; justify-content: space-between; align-items: center; gap: 6px; flex-wrap: wrap; margin-bottom: 8px; }
    .c64-banner {
      border: 2px solid var(--acc); background: var(--sub); padding: 8px 10px; margin-bottom: 10px;
      text-align: center; letter-spacing: 0.5px;
    }
    .c64-banner h1 { color: var(--fg); margin: 0; font-size: 1.25em; letter-spacing: 1px; }
    .c64-sub { font-size: 11px; color: var(--fg); opacity: 0.9; margin-top: 3px; }
    h3 { color: #fff; margin: 0 0 8px; font-size: 1.02em; display: flex; justify-content: space-between; align-items: center; flex-wrap: wrap; gap: 6px; }
    .card { background: var(--sub); padding: 11px; border: 2px solid var(--acc); margin-top: 11px; text-align: left; }
    .wifi-box { border-style: dashed; }
    button, input[type=file], input[type=text], input[type=password], select {
      background: var(--inp); color: #fff; border: 2px solid var(--acc); padding: 7px 9px;
      margin: 3px 0; font-family: monospace; box-sizing: border-box; min-height: 38px; font-size: inherit;
    }
    button { background: var(--btn); color: var(--btntxt); border: 2px solid #fff; font-weight: bold; cursor: pointer; padding: 7px 11px; display: inline-flex; align-items: center; justify-content: center; gap: 5px; }
    button:hover:not(:disabled) { filter: brightness(1.15); }
    button:disabled { opacity: 0.45; cursor: not-allowed; }
    .btn-sec { background: var(--inp); color: #fff; border-color: var(--acc); }
    .btn-warn { background: #9c3b3b; color: #fff; }
    .btn-stop { background: #d64545; color: #fff; }
    .btn-sm { min-height: 30px; padding: 3px 7px; font-size: 12px; }
    .row { display: flex; flex-wrap: wrap; gap: 6px; align-items: center; }
    .row > input, .row > select { flex: 1 1 170px; }
    .net-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(160px, 1fr)); gap: 6px; margin-top: 8px; text-align: left; }
    .net-cell { background: var(--inp); border: 1px solid var(--acc); padding: 6px 8px; font-size: 12px; }
    .net-cell b { color: #fff; display: block; margin-bottom: 2px; font-size: 11px; text-transform: uppercase; opacity: 0.85; }
    .badge { display: inline-flex; align-items: center; gap: 5px; padding: 3px 8px; border: 1px solid var(--acc); background: var(--inp); font-size: 12px; }
    .status { color: var(--ok); font-weight: bold; }
    .err { color: #ff6b6b; font-weight: bold; }
    .now-playing {
      display: none; margin-top: 10px; padding: 9px 11px; background: var(--inp); border: 2px solid var(--ok);
      align-items: center; justify-content: space-between; gap: 8px; flex-wrap: wrap; text-align: left;
    }
    .path-bar { display: flex; align-items: center; justify-content: space-between; gap: 6px; flex-wrap: wrap; background: var(--inp); border: 1px solid var(--acc); padding: 5px 8px; margin-bottom: 6px; font-size: 12px; }
    table { width: 100%; margin-top: 6px; border-collapse: collapse; table-layout: fixed; }
    th, td { border: 1px solid var(--acc); padding: 6px; text-align: left; color: var(--fg); }
    th { background: var(--inp); color: #fff; cursor: pointer; user-select: none; }
    th:hover { filter: brightness(1.15); }
    td.fname { word-break: break-word; }
    td.fsize { width: 70px; text-align: right; white-space: nowrap; font-size: 12px; }
    td.fact { width: 146px; text-align: center; white-space: nowrap; }
    .ico { display: inline-flex; align-items: center; justify-content: center; width: 16px; height: 16px; vertical-align: middle; flex-shrink: 0; }
    .ico svg { width: 15px; height: 15px; display: block; }
    .spin { animation: spin360 1s linear infinite; }
    @keyframes spin360 { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
    .file-ico-wrap { display: inline-flex; align-items: center; gap: 6px; }
    .dir-link { color: #fff; font-weight: bold; cursor: pointer; text-decoration: underline; }
    .prog-wrap { margin-top: 6px; background: var(--inp); border: 1px solid var(--acc); height: 22px; position: relative; overflow: hidden; display: none; }
    .prog-bar { height: 100%; width: 0%; background: var(--ok); transition: width 0.15s ease; }
    .prog-label { position: absolute; inset: 0; display: flex; align-items: center; justify-content: center; gap: 6px; font-size: 12px; font-weight: bold; color: #fff; text-shadow: 1px 1px #000; }
    .op-strip { padding: 6px 8px; background: var(--inp); border: 1px solid var(--acc); font-size: 12px; display: flex; align-items: center; justify-content: space-between; gap: 8px; flex-wrap: wrap; text-align: left; }
    #wifiMsg { margin-top: 8px; font-size: 13px; font-weight: bold; min-height: 18px; }
    .footer { margin-top: 14px; padding-top: 10px; border-top: 1px dashed var(--acc); font-size: 12px; color: var(--fg); line-height: 1.5; text-align: center; }
    .footer a { color: var(--ok); text-decoration: none; font-weight: bold; }
    .footer a:hover { text-decoration: underline; }
  </style>
</head>
<body>
  <svg style="display:none" xmlns="http://www.w3.org/2000/svg">
    <symbol id="i-idle" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2">
      <circle cx="12" cy="12" r="8"/><circle cx="12" cy="12" r="2.5" fill="currentColor"/>
    </symbol>
    <symbol id="i-play" viewBox="0 0 24 24" fill="currentColor" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round">
      <polygon points="6,4 20,12 6,20"/>
    </symbol>
    <symbol id="i-stop" viewBox="0 0 24 24" fill="currentColor">
      <rect x="6" y="6" width="12" height="12" rx="1"/>
    </symbol>
    <symbol id="i-tape" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <rect x="2" y="5" width="20" height="14" rx="2"/><circle cx="8" cy="12" r="2.2"/><circle cx="16" cy="12" r="2.2"/><line x1="8" y1="14.2" x2="16" y2="14.2"/>
    </symbol>
    <symbol id="i-folder" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round">
      <path d="M3 6h6l2 2h10v10a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>
    </symbol>
    <symbol id="i-upload" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M12 16V4"/><path d="M7 9l5-5 5 5"/><path d="M4 19h16"/>
    </symbol>
    <symbol id="i-spin" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round">
      <path d="M12 3a9 9 0 1 1-6.36 2.64"/>
    </symbol>
    <symbol id="i-ok" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
      <path d="M4 12.5l5 5L20 6.5"/>
    </symbol>
    <symbol id="i-err" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round">
      <circle cx="12" cy="12" r="9"/><line x1="15" y1="9" x2="9" y2="15"/><line x1="9" y1="9" x2="15" y2="15"/>
    </symbol>
    <symbol id="i-trash" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <polyline points="3 6 5 6 21 6"/><path d="M19 6l-1 14H6L5 6"/><path d="M10 11v6"/><path d="M14 11v6"/>
    </symbol>
    <symbol id="i-edit" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
      <path d="M12 20h9"/><path d="M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4Z"/>
    </symbol>
  </svg>

  <div class="container">
    <div class="topbar">
      <div class="row">
        <span class="badge" id="playBadge"><span class="ico"><svg><use href="#i-idle"/></svg></span><span>IDLE</span></span>
        <span class="badge" id="motorBadge">MOTOR C64: OFF</span>
      </div>
      <div>
        <select id="themeSelect" class="btn-sec btn-sm" onchange="applyTheme(this.value)" title="Seleziona tema grafico" style="min-height:30px;padding:2px 6px;font-size:12px;">
          <option value="theme-c64">Tema: C64</option>
          <option value="theme-dark">Tema: Scuro</option>
          <option value="theme-amiga">Tema: Amiga</option>
          <option value="theme-phosphor">Tema: Fosforo Verde</option>
          <option value="theme-amber">Tema: Ambra Retro</option>
        </select>
        <button type="button" class="btn-sec btn-sm" onclick="toggleFont()">Testo A+/A</button>
      </div>
    </div>

    <div class="c64-banner">
      <h1>**** TAPUINO NEXT V2 ****</h1>
      <div class="c64-sub">COMMODORE 64 DATASSETTE EMULATOR &bull; ESP32 WEB CONSOLE</div>
    </div>

    <div class="op-strip">
      <div style="display:flex;align-items:center;gap:7px;">
        <span class="ico" id="opIcon"><svg><use href="#i-idle"/></svg></span>
        <span>Stato: <span id="status" class="status">Pronto</span></span>
      </div>
      <span id="sdSpaceLabel" style="font-size:12px;">SD: --</span>
    </div>

    <div class="now-playing" id="nowPlayingBar">
      <div style="display:flex;align-items:center;gap:8px;">
        <span class="ico spin" style="color:var(--ok)"><svg><use href="#i-tape"/></svg></span>
        <div>
          <div style="font-size:11px;opacity:0.85;">IN RIPRODUZIONE SU C64</div>
          <b id="nowPlayingName" style="color:#fff;">--</b> <span id="nowPlayingCounter" style="font-size:12px;">(CNT: 000)</span>
        </div>
      </div>
      <button type="button" class="btn-stop" onclick="stopPlayback()"><span class="ico"><svg><use href="#i-stop"/></svg></span> Stop</button>
    </div>

    <div class="net-grid">
      <div class="net-cell"><b>Rete AP</b><span id="apCell">TapuinoNext (192.168.4.1)</span></div>
      <div class="net-cell"><b>Rete Wi-Fi STA</b><span id="staCell">Verifica...</span></div>
      <div class="net-cell"><b>Hostname</b><span id="hostnameLabel">TapuinoNext</span></div>
      <div class="net-cell"><b>MAC STA / AP</b><span id="macCell">--</span></div>
    </div>

    <div class="card">
      <h3>
        <span id="sdCardTitle">Archivio MicroSD (0 file)</span>
        <button type="button" class="btn-sec btn-sm" id="btnReload" onclick="loadFiles()">Aggiorna</button>
      </h3>

      <div class="path-bar">
        <span>Cartella: <b id="curDirLabel">/</b></span>
        <button type="button" class="btn-sec btn-sm" id="btnDirUp" onclick="goDirUp()" style="display:none;">&uarr; Cartella Superiore</button>
      </div>

      <div class="row">
        <input type="file" id="fileInput" accept=".tap,.TAP,.prg,.PRG">
        <button type="button" id="btnUpload" onclick="uploadFile()"><span class="ico"><svg><use href="#i-upload"/></svg></span> Invia a SD</button>
      </div>
      <div class="prog-wrap" id="progWrap">
        <div class="prog-bar" id="progBar"></div>
        <div class="prog-label" id="progLabel"><span class="ico spin"><svg><use href="#i-spin"/></svg></span><span>0%</span></div>
      </div>

      <div class="row" style="margin-top:6px;">
        <input type="text" id="filterInput" placeholder="Filtra file (.tap / .prg) o cartelle..." oninput="renderFiles()">
      </div>

      <table id="fileTable">
        <tr>
          <th onclick="toggleSort('name')">Nome <span id="sortNameInd">&uarr;</span></th>
          <th onclick="toggleSort('size')" style="width:70px;text-align:right;">Dim. <span id="sortSizeInd"></span></th>
          <th style="width:146px;text-align:center;">Azioni</th>
        </tr>
      </table>
    </div>

    <div class="card wifi-box">
      <h3>Configurazione Wi-Fi (NVS Permanente)</h3>
      <div class="row">
        <select id="netSelect" onchange="selectNet()">
          <option value="">-- Seleziona rete rilevata --</option>
        </select>
        <button type="button" class="btn-sec" onclick="scanWifi()">Cerca Reti</button>
      </div>
      <div class="row">
        <input type="text" id="wifiSsid" placeholder="Nome Rete (SSID)">
        <input type="password" id="wifiPass" placeholder="Password Wi-Fi">
        <button type="button" class="btn-sec" id="btnPass" onclick="togglePass()">Mostra</button>
      </div>
      <div class="row" style="margin-top:6px;">
        <button type="button" onclick="saveWifi()">Salva e Riavvia</button>
        <button type="button" class="btn-sec" id="btnReboot" onclick="rebootEsp()">Riavvia Scheda</button>
        <button type="button" class="btn-warn" onclick="resetWifi()">Reset Wi-Fi</button>
      </div>
      <div id="wifiMsg"></div>
    </div>

    <div class="card">
      <h3>Aggiornamento Firmware OTA (.bin)</h3>
      <div class="row">
        <input type="file" id="otaInput" accept=".bin">
        <button type="button" id="btnOta" onclick="uploadOta()"><span class="ico"><svg><use href="#i-upload"/></svg></span> Aggiorna Firmware</button>
      </div>
      <div class="prog-wrap" id="otaProgWrap">
        <div class="prog-bar" id="otaProgBar"></div>
        <div class="prog-label" id="otaProgLabel"><span>0%</span></div>
      </div>
    </div>

    <div class="footer">
      Progetto originale <a href="https://github.com/sweetlilmre/TapuinoNext" target="_blank" rel="noopener noreferrer">https://github.com/sweetlilmre/TapuinoNext</a> modificato da Manuel Conti aka LordFennec <a href="https://www.fennecelectronics.it" target="_blank" rel="noopener noreferrer" onclick="return confirmExternal(event, this.href)">www.fennecelectronics.it</a>
    </div>
  </div>

  <script>
    let cachedEntries = [];
    let currentDir = "/";
    let sortBy = "name";
    let sortAsc = true;
    let busyPlaying = false;
    let activeFile = "";
    let fileStates = {};

    function iconHtml(symbolId, extraClass) {
      let cls = extraClass ? (' class="' + extraClass + '"') : '';
      return '<span class="ico"><svg' + cls + '><use href="#' + symbolId + '"/></svg></span>';
    }

    function setOpIcon(symbolId, color, spin) {
      let wrap = document.getElementById('opIcon');
      wrap.innerHTML = '<svg class="' + (spin ? 'spin' : '') + '"><use href="#' + symbolId + '"/></svg>';
      wrap.style.color = color || 'var(--fg)';
    }

    const THEMES = ['theme-c64', 'theme-dark', 'theme-amiga', 'theme-phosphor', 'theme-amber'];
    function applyTheme(themeName) {
      let chosen = THEMES.includes(themeName) ? themeName : 'theme-c64';
      THEMES.forEach(t => document.body.classList.remove(t));
      document.body.classList.add(chosen);
      let sel = document.getElementById('themeSelect');
      if (sel) sel.value = chosen;
      try { localStorage.setItem('tapuinoTheme', chosen); } catch (e) {}
    }
    function toggleFont() { document.body.classList.toggle('lg'); }
    function togglePass() {
      let p = document.getElementById('wifiPass');
      let b = document.getElementById('btnPass');
      p.type = (p.type === 'password') ? 'text' : 'password';
      b.innerText = (p.type === 'password') ? 'Mostra' : 'Nascondi';
    }

    function setMsg(txt, isErr, iconId, spinning) {
      let el = document.getElementById('wifiMsg');
      el.className = isErr ? 'err' : 'status';
      el.innerHTML = iconHtml(iconId || (isErr ? 'i-err' : 'i-ok'), spinning ? 'spin' : '') + ' ' + txt;
      let st = document.getElementById('status');
      st.className = isErr ? 'err' : 'status';
      st.innerText = txt;
      setOpIcon(iconId || (isErr ? 'i-err' : 'i-ok'), isErr ? '#ff6b6b' : 'var(--ok)', !!spinning);
    }

    function setBar(wrapId, barId, lblId, pct, text, state) {
      let wrap = document.getElementById(wrapId);
      let bar = document.getElementById(barId);
      let lbl = document.getElementById(lblId);
      if (state === 'hide') { wrap.style.display = 'none'; bar.style.width = '0%'; return; }
      wrap.style.display = 'block';
      let c = Math.max(0, Math.min(100, pct));
      bar.style.width = c + '%';
      if (state === 'done') {
        bar.style.background = 'var(--ok)';
        lbl.innerHTML = iconHtml('i-ok', '') + '<span>' + text + '</span>';
      } else if (state === 'error') {
        bar.style.background = '#ff6b6b';
        lbl.innerHTML = iconHtml('i-err', '') + '<span>' + text + '</span>';
      } else {
        bar.style.background = 'var(--acc)';
        lbl.innerHTML = iconHtml('i-spin', 'spin') + '<span>' + text + '</span>';
      }
    }

    function updatePlayingUI(playing, motorOn, counterVal, activePath) {
      let wasPlaying = busyPlaying;
      busyPlaying = !!playing;
      if (activePath) activeFile = activePath;
      if (!busyPlaying && wasPlaying && activeFile) {
        fileStates[activeFile] = 'done';
      }

      let badge = document.getElementById('playBadge');
      let npBar = document.getElementById('nowPlayingBar');
      if (busyPlaying) {
        badge.innerHTML = iconHtml('i-play', '') + '<span>PLAYING TAP...</span>';
        badge.style.color = '#ff6b6b';
        npBar.style.display = 'flex';
        document.getElementById('nowPlayingName').innerText = activeFile || 'File TAP';
        document.getElementById('nowPlayingCounter').innerText = '(CNT: ' + String(counterVal || 0).padStart(3, '0') + ')';
        setOpIcon('i-play', '#ff6b6b', false);
      } else {
        badge.innerHTML = iconHtml('i-idle', '') + '<span>IDLE</span>';
        badge.style.color = 'var(--ok)';
        npBar.style.display = 'none';
      }

      let mb = document.getElementById('motorBadge');
      mb.innerText = motorOn ? 'MOTOR C64: ON' : 'MOTOR C64: OFF';
      mb.style.color = motorOn ? 'var(--ok)' : 'var(--fg)';

      ['btnUpload', 'btnReboot', 'btnOta'].forEach(id => {
        let b = document.getElementById(id);
        if (b) b.disabled = busyPlaying;
      });
      renderFiles();
    }

    function fullItemPath(name) {
      return (currentDir === "/") ? ("/" + name) : (currentDir + "/" + name);
    }

    function toggleSort(col) {
      if (sortBy === col) sortAsc = !sortAsc;
      else { sortBy = col; sortAsc = true; }
      renderFiles();
    }

    function renderFiles() {
      let q = (document.getElementById('filterInput').value || '').toLowerCase();
      let table = document.getElementById('fileTable');
      let nInd = (sortBy === 'name') ? (sortAsc ? '&uarr;' : '&darr;') : '';
      let sInd = (sortBy === 'size') ? (sortAsc ? '&uarr;' : '&darr;') : '';
      table.innerHTML = `<tr><th onclick="toggleSort('name')">Nome ${nInd}</th><th onclick="toggleSort('size')" style="width:70px;text-align:right;">Dim. ${sInd}</th><th style="width:146px;text-align:center;">Azioni</th></tr>`;

      let filtered = cachedEntries.filter(e => e.name.toLowerCase().includes(q));
      filtered.sort((a, b) => {
        if (a.isDir !== b.isDir) return a.isDir ? -1 : 1;
        let cmp = (sortBy === 'size') ? ((a.size || 0) - (b.size || 0)) : a.name.localeCompare(b.name);
        return sortAsc ? cmp : -cmp;
      });

      let tapCount = cachedEntries.filter(e => !e.isDir).length;
      let totalKb = Math.round(cachedEntries.reduce((acc, e) => acc + (e.isDir ? 0 : (e.size || 0)), 0) / 1024);
      document.getElementById('sdCardTitle').innerText = `Archivio MicroSD (${tapCount} file - ${totalKb} KB)`;
      document.getElementById('curDirLabel').innerText = currentDir;
      document.getElementById('btnDirUp').style.display = (currentDir !== "/") ? "inline-flex" : "none";

      if (filtered.length === 0) {
        let r = table.insertRow();
        let c = r.insertCell(0);
        c.colSpan = 3;
        c.innerText = cachedEntries.length ? 'Nessun elemento corrisponde al filtro.' : 'Cartella vuota (o MicroSD assente).';
        return;
      }

      filtered.forEach(item => {
        let row = table.insertRow();
        let c0 = row.insertCell(0);
        c0.className = 'fname';
        let itemPath = fullItemPath(item.name);

        if (item.isDir) {
          c0.innerHTML = '<span class="file-ico-wrap">' + iconHtml('i-folder', '') + '<span class="dir-link">' + item.name + '/</span></span>';
          c0.onclick = () => openDir(item.name);
        } else {
          let st = fileStates[itemPath];
          let ico = iconHtml('i-tape', '');
          if (st === 'playing' && busyPlaying) ico = iconHtml('i-play', '');
          else if (st === 'done') ico = '<span style="color:var(--ok)">' + iconHtml('i-ok', '') + '</span>';
          else if (st === 'error') ico = '<span style="color:#ff6b6b">' + iconHtml('i-err', '') + '</span>';
          c0.innerHTML = '<span class="file-ico-wrap">' + ico + '<span>' + item.name + '</span></span>';
        }

        let c1 = row.insertCell(1);
        c1.className = 'fsize';
        c1.innerText = item.isDir ? 'DIR' : (Math.max(1, Math.round((item.size || 0) / 1024)) + ' KB');

        let c2 = row.insertCell(2);
        c2.className = 'fact';
        if (item.isDir) {
          let bOpen = document.createElement('button');
          bOpen.type = 'button';
          bOpen.className = 'btn-sec btn-sm';
          bOpen.innerText = 'Apri';
          bOpen.onclick = () => openDir(item.name);
          c2.appendChild(bOpen);
        } else {
          let isPrg = item.name.toLowerCase().endsWith('.prg');
          let bPlay = document.createElement('button');
          bPlay.type = 'button';
          bPlay.className = 'btn-sm';
          bPlay.title = isPrg ? 'Invia PRG via fastloader (attendi handshake 0xca65 sul C64)' : 'Avvia riproduzione TAP';
          bPlay.innerHTML = iconHtml('i-play', '') + (isPrg ? 'PRG' : 'Play');
          bPlay.disabled = busyPlaying;
          bPlay.onclick = () => playFile(itemPath);

          let bRen = document.createElement('button');
          bRen.type = 'button';
          bRen.className = 'btn-sec btn-sm';
          bRen.title = 'Rinomina';
          bRen.innerHTML = iconHtml('i-edit', '');
          bRen.disabled = busyPlaying;
          bRen.onclick = () => renameFile(itemPath, item.name);

          let bDel = document.createElement('button');
          bDel.type = 'button';
          bDel.className = 'btn-warn btn-sm';
          bDel.title = 'Elimina';
          bDel.innerHTML = iconHtml('i-trash', '');
          bDel.disabled = busyPlaying;
          bDel.onclick = () => deleteFile(itemPath, item.name);

          c2.appendChild(bPlay);
          c2.appendChild(document.createTextNode(' '));
          c2.appendChild(bRen);
          c2.appendChild(document.createTextNode(' '));
          c2.appendChild(bDel);
        }
      });
    }

    function openDir(name) {
      currentDir = fullItemPath(name);
      loadFiles();
    }

    function goDirUp() {
      if (currentDir === "/") return;
      let idx = currentDir.lastIndexOf("/");
      currentDir = (idx <= 0) ? "/" : currentDir.substring(0, idx);
      loadFiles();
    }

    function loadFiles() {
      setOpIcon('i-spin', 'var(--acc)', true);
      fetch('/files?dir=' + encodeURIComponent(currentDir))
        .then(r => {
          if (r.status === 503) { updatePlayingUI(true, false, 0, activeFile); throw new Error("Busy"); }
          if (!r.ok) throw new Error("SD");
          return r.json();
        })
        .then(data => {
          cachedEntries = data || [];
          renderFiles();
          if (!busyPlaying) setOpIcon('i-idle', 'var(--ok)', false);
        })
        .catch(err => {
          if (err.message !== "Busy") {
            document.getElementById('status').innerText = "SD non pronta o assente";
            setOpIcon('i-err', '#ff6b6b', false);
          }
        });
    }

    function loadWifiInfo() {
      fetch('/wifiinfo')
        .then(r => r.json())
        .then(info => {
          if (info.hostname) document.getElementById('hostnameLabel').innerText = info.hostname;
          document.getElementById('apCell').innerText = `${info.apSsid || 'TapuinoNext'} (${info.apIp || '192.168.4.1'})`;
          document.getElementById('staCell').innerHTML = info.staConnected
            ? `<span class="status">Connesso (${info.staIp})</span>`
            : `Non connesso (${info.savedSsid || 'nessuna rete'})`;
          document.getElementById('macCell').innerText = `${info.macSta || '--'} / ${info.macAp || '--'}`;
          if (info.sdTotalMb > 0) {
            document.getElementById('sdSpaceLabel').innerText = `SD: ${info.sdFreeMb} MB liberi / ${info.sdTotalMb} MB`;
          }
          if (info.savedSsid && !document.getElementById('wifiSsid').value) {
            document.getElementById('wifiSsid').value = info.savedSsid;
          }
          updatePlayingUI(info.isTapPlaying, info.motorOn, info.counter, info.activeFile);
        })
        .catch(() => {});
    }

    function playFile(path) {
      activeFile = path;
      fileStates[path] = 'playing';
      renderFiles();
      fetch('/play?file=' + encodeURIComponent(path), { method: 'POST' })
        .then(async r => {
          let msg = await r.text();
          if (!r.ok) throw new Error(msg);
          document.getElementById('status').className = 'status';
          document.getElementById('status').innerText = "Avviato: " + path;
          updatePlayingUI(true, false, 0, path);
        })
        .catch(err => {
          fileStates[path] = 'error';
          renderFiles();
          setMsg(err.message || "Errore avvio TAP", true, 'i-err', false);
        });
    }

    function stopPlayback() {
      fetch('/stop', { method: 'POST' })
        .then(async r => {
          let msg = await r.text();
          setMsg(msg || "Stop inviato", !r.ok, r.ok ? 'i-stop' : 'i-err', false);
          updatePlayingUI(false, false, 0, activeFile);
        })
        .catch(() => setMsg("Errore invio comando Stop", true, 'i-err', false));
    }

    function deleteFile(path, name) {
      if (!confirm("Eliminare definitivamente '" + name + "' dalla MicroSD?")) return;
      fetch('/delete?file=' + encodeURIComponent(path), { method: 'POST' })
        .then(async r => {
          let msg = await r.text();
          if (!r.ok) throw new Error(msg);
          setMsg(msg, false, 'i-ok', false);
          loadFiles();
        })
        .catch(err => setMsg(err.message || "Errore eliminazione file", true, 'i-err', false));
    }

    function renameFile(path, oldName) {
      let ext = oldName.toLowerCase().endsWith(".prg") ? ".prg" : ".tap";
      let newName = prompt("Nuovo nome per '" + oldName + "' (mantieni " + ext + "):", oldName);
      if (!newName || newName.trim() === "" || newName.trim() === oldName) return;
      newName = newName.trim();
      if (!newName.toLowerCase().endsWith(".tap") && !newName.toLowerCase().endsWith(".prg")) newName += ext;
      let newPath = fullItemPath(newName);
      fetch('/rename?old=' + encodeURIComponent(path) + '&new=' + encodeURIComponent(newPath), { method: 'POST' })
        .then(async r => {
          let msg = await r.text();
          if (!r.ok) throw new Error(msg);
          setMsg(msg, false, 'i-ok', false);
          loadFiles();
        })
        .catch(err => setMsg(err.message || "Errore rinomina file", true, 'i-err', false));
    }

    function uploadFile() {
      let fileInput = document.getElementById('fileInput');
      if (fileInput.files.length === 0) {
        setMsg('Seleziona prima un file .TAP!', true, 'i-err', false);
        return;
      }
      let file = fileInput.files[0];
      let formData = new FormData();
      formData.append("file", file);

      let xhr = new XMLHttpRequest();
      xhr.open('POST', '/upload?dir=' + encodeURIComponent(currentDir), true);
      setBar('progWrap', 'progBar', 'progLabel', 0, 'Caricamento 0%', 'uploading');
      setMsg("Caricamento " + file.name + " su SD...", false, 'i-upload', true);

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          let pct = Math.round((e.loaded / e.total) * 100);
          setBar('progWrap', 'progBar', 'progLabel', pct, 'Caricamento ' + pct + '%', 'uploading');
        }
      };
      xhr.onload = function() {
        if (xhr.status >= 200 && xhr.status < 300) {
          setBar('progWrap', 'progBar', 'progLabel', 100, '100% Salvato su SD', 'done');
          setMsg(xhr.responseText || "File salvato su MicroSD!", false, 'i-ok', false);
          fileInput.value = '';
          loadFiles();
          setTimeout(() => setBar('progWrap', 'progBar', 'progLabel', 0, '', 'hide'), 3000);
        } else {
          setBar('progWrap', 'progBar', 'progLabel', 100, 'Errore upload', 'error');
          setMsg(xhr.responseText || 'Errore upload su SD', true, 'i-err', false);
        }
      };
      xhr.onerror = function() {
        setBar('progWrap', 'progBar', 'progLabel', 100, 'Errore di rete', 'error');
        setMsg('Errore di rete durante il caricamento', true, 'i-err', false);
      };
      xhr.send(formData);
    }

    function uploadOta() {
      let otaInput = document.getElementById('otaInput');
      if (otaInput.files.length === 0) {
        setMsg('Seleziona prima il file firmware .bin!', true, 'i-err', false);
        return;
      }
      let file = otaInput.files[0];
      if (!confirm("Avviare l'aggiornamento firmware OTA con '" + file.name + "'? La scheda si riavviera al termine.")) return;

      let formData = new FormData();
      formData.append("firmware", file);
      let xhr = new XMLHttpRequest();
      xhr.open('POST', '/ota', true);
      setBar('otaProgWrap', 'otaProgBar', 'otaProgLabel', 0, 'Flash OTA 0%', 'uploading');
      setMsg("Aggiornamento firmware OTA in corso...", false, 'i-spin', true);

      xhr.upload.onprogress = function(e) {
        if (e.lengthComputable) {
          let pct = Math.round((e.loaded / e.total) * 100);
          setBar('otaProgWrap', 'otaProgBar', 'otaProgLabel', pct, 'Flash OTA ' + pct + '%', 'uploading');
        }
      };
      xhr.onload = function() {
        if (xhr.status >= 200 && xhr.status < 300) {
          setBar('otaProgWrap', 'otaProgBar', 'otaProgLabel', 100, '100% Riavvio...', 'done');
          setMsg(xhr.responseText || "Firmware aggiornato! Riavvio in corso...", false, 'i-ok', false);
        } else {
          setBar('otaProgWrap', 'otaProgBar', 'otaProgLabel', 100, 'Errore OTA', 'error');
          setMsg(xhr.responseText || "Errore aggiornamento OTA", true, 'i-err', false);
        }
      };
      xhr.onerror = function() {
        setBar('otaProgWrap', 'otaProgBar', 'otaProgLabel', 100, 'Errore rete OTA', 'error');
        setMsg("Errore di rete durante OTA", true, 'i-err', false);
      };
      xhr.send(formData);
    }

    function scanWifi() {
      setMsg("Scansione reti Wi-Fi...", false, 'i-spin', true);
      fetch('/scan')
        .then(r => r.json())
        .then(nets => {
          let sel = document.getElementById('netSelect');
          sel.innerHTML = '<option value="">-- Seleziona rete rilevata --</option>';
          nets.forEach(n => {
            let opt = document.createElement('option');
            opt.value = n.ssid;
            opt.innerText = `${n.ssid} (${n.rssi} dBm)`;
            sel.appendChild(opt);
          });
          setMsg("Scansione completata (" + nets.length + " reti)", false, 'i-ok', false);
        })
        .catch(() => setMsg("Errore scansione reti", true, 'i-err', false));
    }

    function selectNet() {
      let val = document.getElementById('netSelect').value;
      if (val) document.getElementById('wifiSsid').value = val;
    }

    function saveWifi() {
      let s = document.getElementById('wifiSsid').value.trim();
      let p = document.getElementById('wifiPass').value;
      if (!s) {
        setMsg("Inserisci o seleziona il nome della rete Wi-Fi (SSID)!", true, 'i-err', false);
        return;
      }
      setMsg("Salvataggio credenziali in corso...", false, 'i-spin', true);
      let params = 'ssid=' + encodeURIComponent(s) + '&pass=' + encodeURIComponent(p);
      fetch('/savewifi?' + params, {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: params
      })
        .then(async r => {
          let txt = await r.text();
          if (!r.ok) throw new Error(txt || "Errore salvataggio");
          setMsg(txt, false, 'i-ok', false);
        })
        .catch(err => setMsg(err.message || "Errore comunicazione con ESP32", true, 'i-err', false));
    }

    function rebootEsp() {
      if (!confirm("Vuoi riavviare la scheda ESP32 ora?")) return;
      setMsg("Riavvio della scheda in corso...", false, 'i-spin', true);
      fetch('/reboot', { method: 'POST' })
        .then(async r => setMsg(await r.text(), !r.ok, r.ok ? 'i-spin' : 'i-err', r.ok))
        .catch(() => setMsg("Riavvio inviato...", false, 'i-spin', true));
    }

    function resetWifi() {
      if (!confirm("Vuoi cancellare le credenziali Wi-Fi salvate e riavviare la scheda?")) return;
      setMsg("Cancellazione credenziali salvate...", false, 'i-spin', true);
      fetch('/resetwifi', { method: 'POST' })
        .then(async r => setMsg(await r.text(), !r.ok, r.ok ? 'i-ok' : 'i-err', false))
        .catch(() => setMsg("Errore durante il reset Wi-Fi", true, 'i-err', false));
    }

    function confirmExternal(ev, url) {
      if (!confirm("Stai per aprire un link esterno (" + url + "). Vuoi procedere?")) {
        if (ev) ev.preventDefault();
        return false;
      }
      return true;
    }

    window.onload = function() {
      let savedTheme = 'theme-c64';
      try { savedTheme = localStorage.getItem('tapuinoTheme') || 'theme-c64'; } catch (e) {}
      applyTheme(savedTheme);
      loadFiles();
      loadWifiInfo();
      setInterval(loadWifiInfo, 3000);
    };
  </script>
</body>
</html>
)rawliteral";

static String escapeJson(const String& s) {
  String out = s;
  out.replace("\\", "\\\\");
  out.replace("\"", "\\\"");
  return out;
}

static String sanitizePath(String p) {
  p.trim();
  if (p.length() == 0) return "/";
  if (!p.startsWith("/")) p = "/" + p;
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  if (p.indexOf("..") >= 0) return "/";
  if (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleWifiInfo() {
  wifiPrefs.begin(PREF_NAMESPACE, true);
  String savedSsid = wifiPrefs.getString("ssid", "");
  wifiPrefs.end();

  uint64_t totalBytes = SD_MMC.totalBytes();
  uint64_t usedBytes = SD_MMC.usedBytes();
  uint32_t totalMb = (uint32_t)(totalBytes / (1024ULL * 1024ULL));
  uint32_t freeMb = (totalBytes >= usedBytes) ? (uint32_t)((totalBytes - usedBytes) / (1024ULL * 1024ULL)) : 0;

  bool motorLive = isTapPlaying ? (bool)c64MotorActive : (digitalRead(C64_MOTOR_PIN) != 0);

  String json = "{";
  json += "\"savedSsid\":\"" + escapeJson(savedSsid) + "\",";
  json += "\"apSsid\":\"" + escapeJson(String(AP_SSID)) + "\",";
  json += "\"apIp\":\"" + WiFi.softAPIP().toString() + "\",";
  json += "\"hostname\":\"" + escapeJson(String(WiFi.getHostname() ? WiFi.getHostname() : AP_SSID)) + "\",";
  json += "\"macSta\":\"" + WiFi.macAddress() + "\",";
  json += "\"macAp\":\"" + WiFi.softAPmacAddress() + "\",";
  json += "\"staConnected\":" + String(WiFi.status() == WL_CONNECTED ? "true" : "false") + ",";
  json += "\"staIp\":\"" + (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("")) + "\",";
  json += "\"isTapPlaying\":" + String(isTapPlaying ? "true" : "false") + ",";
  json += "\"motorOn\":" + String(motorLive ? "true" : "false") + ",";
  json += "\"counter\":" + String((unsigned int)tapCounterCurrent) + ",";
  json += "\"activeFile\":\"" + escapeJson(String(remoteFilePath.c_str())) + "\",";
  json += "\"sdTotalMb\":" + String(totalMb) + ",";
  json += "\"sdFreeMb\":" + String(freeMb);
  json += "}";
  server.send(200, "application/json", json);
}

void handleGetFiles() {
  if (isTapPlaying) {
    server.send(503, "application/json", "[]");
    return;
  }

  String dirPath = sanitizePath(server.hasArg("dir") ? server.arg("dir") : "/");
  File root = SD_MMC.open(dirPath);
  if (!root || !root.isDirectory()) {
    server.send(500, "application/json", "[]");
    return;
  }

  String json = "[";
  File entry = root.openNextFile();
  bool first = true;
  while (entry) {
    String name = String(entry.name());
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.length() > 0 && !name.startsWith(".")) {
      bool isDir = entry.isDirectory();
      if (isDir || name.endsWith(".tap") || name.endsWith(".TAP") || name.endsWith(".prg") || name.endsWith(".PRG")) {
        if (!first) json += ",";
        json += "{\"name\":\"" + escapeJson(name) + "\",\"isDir\":" + String(isDir ? "true" : "false") + ",\"size\":" + String(isDir ? 0UL : (unsigned long)entry.size()) + "}";
        first = false;
      }
    }
    entry = root.openNextFile();
  }
  root.close();
  json += "]";
  server.send(200, "application/json", json);
}

File uploadFileObj;
void handleFileUpload() {
  if (isTapPlaying) return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    String dirPath = sanitizePath(server.hasArg("dir") ? server.arg("dir") : "/");
    String filename = upload.filename;
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    String fullPath = (dirPath == "/") ? ("/" + filename) : (dirPath + "/" + filename);
    uploadFileObj = SD_MMC.open(fullPath, FILE_WRITE);
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFileObj) {
      uploadFileObj.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFileObj) {
      uploadFileObj.close();
    }
  }
}

void handleUploadPost() {
  if (isTapPlaying) {
    server.send(503, "text/plain", "Impossibile caricare durante la riproduzione");
    return;
  }
  server.send(200, "text/plain", "File salvato su MicroSD!");
}

void handlePlay() {
  if (isTapPlaying) {
    server.send(409, "text/plain", "Un file TAP e' gia' in riproduzione!");
    return;
  }
  if (server.hasArg("file")) {
    String filename = sanitizePath(server.arg("file"));
    remoteFilePath = std::string(filename.c_str());
    remoteStopRequested = false;
    remotePlayRequested = true;
    server.send(200, "text/plain", "Avvio riproduzione di: " + filename);
  } else {
    server.send(400, "text/plain", "Parametro 'file' mancante");
  }
}

void handleStop() {
  if (!isTapPlaying) {
    server.send(200, "text/plain", "Nessun TAP in esecuzione");
    return;
  }
  remoteStopRequested = true;
  server.send(200, "text/plain", "Stop riproduzione inviato a TapuinoNext");
}

void handleDeleteFile() {
  if (isTapPlaying) {
    server.send(409, "text/plain", "Impossibile eliminare file durante la riproduzione");
    return;
  }
  if (!server.hasArg("file")) {
    server.send(400, "text/plain", "Parametro 'file' mancante");
    return;
  }
  String path = sanitizePath(server.arg("file"));
  if (path == "/" || !SD_MMC.exists(path)) {
    server.send(404, "text/plain", "File non trovato");
    return;
  }
  if (SD_MMC.remove(path)) {
    server.send(200, "text/plain", "Eliminato: " + path);
  } else {
    server.send(500, "text/plain", "Errore eliminazione file");
  }
}

void handleRenameFile() {
  if (isTapPlaying) {
    server.send(409, "text/plain", "Impossibile rinominare durante la riproduzione");
    return;
  }
  if (!server.hasArg("old") || !server.hasArg("new")) {
    server.send(400, "text/plain", "Parametri mancanti");
    return;
  }
  String oldPath = sanitizePath(server.arg("old"));
  String newPath = sanitizePath(server.arg("new"));
  if (!SD_MMC.exists(oldPath)) {
    server.send(404, "text/plain", "File originale non trovato");
    return;
  }
  if (SD_MMC.exists(newPath)) {
    server.send(409, "text/plain", "Esiste gia' un file con questo nome");
    return;
  }
  if (SD_MMC.rename(oldPath, newPath)) {
    server.send(200, "text/plain", "Rinominato in: " + newPath);
  } else {
    server.send(500, "text/plain", "Errore durante la rinomina");
  }
}

void handleOtaUpload() {
  if (isTapPlaying) return;
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    otaUploadOk = false;
    Serial.printf("[OTA] Avvio aggiornamento firmware: %s\n", upload.filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      otaUploadOk = true;
      Serial.printf("[OTA] Completato (%u byte). Riavvio...\n", (unsigned int)upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  }
}

void handleOtaPost() {
  if (isTapPlaying) {
    server.send(409, "text/plain", "Impossibile aggiornare durante la riproduzione");
    return;
  }
  if (otaUploadOk) {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", "Firmware OTA aggiornato con successo! Riavvio ESP32...");
    delay(1000);
    ESP.restart();
  } else {
    server.send(500, "text/plain", "Aggiornamento OTA fallito (verifica il file .bin)");
  }
}

void handleScanWifi() {
  if (isTapPlaying) {
    server.send(503, "application/json", "[]");
    return;
  }
  int n = WiFi.scanNetworks();
  String json = "[";
  bool first = true;
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) continue;
    if (!first) json += ",";
    json += "{\"ssid\":\"" + escapeJson(s) + "\",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
    first = false;
  }
  WiFi.scanDelete();
  json += "]";
  server.send(200, "application/json", json);
}

void handleSaveWifi() {
  if (!server.hasArg("ssid")) {
    server.send(400, "text/plain", "SSID mancante");
    return;
  }
  String newSsid = server.arg("ssid");
  newSsid.trim();
  String newPass = server.hasArg("pass") ? server.arg("pass") : "";

  if (newSsid.length() == 0) {
    server.send(400, "text/plain", "SSID non valido");
    return;
  }

  if (!wifiPrefs.begin(PREF_NAMESPACE, false)) {
    server.send(500, "text/plain", "Errore memoria NVS ESP32");
    return;
  }
  wifiPrefs.putString("ssid", newSsid);
  wifiPrefs.putString("pass", newPass);
  String checkSsid = wifiPrefs.getString("ssid", "");
  wifiPrefs.end();

  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Credenziali salvate (" + checkSsid + ")! Riavvio ESP32 in corso...");
  delay(1200);
  ESP.restart();
}

void resetWiFiCredentials() {
  wifiPrefs.begin(PREF_NAMESPACE, false);
  wifiPrefs.clear();
  wifiPrefs.end();
  Serial.println("[WiFi] Credenziali salvate cancellate dalla NVS.");
}

String getWiFiStatusSummary() {
  if (WiFi.status() == WL_CONNECTED) {
    return "STA:" + WiFi.localIP().toString();
  }
  return "AP:" + WiFi.softAPIP().toString();
}

void handleResetWifi() {
  resetWiFiCredentials();
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Credenziali rimosse! Riavvio della scheda in corso...");
  delay(1200);
  ESP.restart();
}

void handleReboot() {
  if (isTapPlaying) {
    server.send(409, "text/plain", "Impossibile riavviare durante la riproduzione TAP");
    return;
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", "Riavvio ESP32 in corso...");
  delay(800);
  ESP.restart();
}

void webServerTask(void* parameter) {
  for (;;) {
    server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(5));
  }
}

void initWebInterface() {
  wifiPrefs.begin(PREF_NAMESPACE, true);
  String savedSsid = wifiPrefs.getString("ssid", "");
  String savedPass = wifiPrefs.getString("pass", "");
  wifiPrefs.end();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setHostname(AP_SSID);
  Serial.printf("[WiFi] Hostname impostato su: %s\n", AP_SSID);
  bool apOk = (strlen(AP_PASS) >= 8)
    ? WiFi.softAP(AP_SSID, AP_PASS)
    : WiFi.softAP(AP_SSID);

  Serial.printf("[WiFi] AP '%s' %s | IP AP: http://", AP_SSID, apOk ? "attivo" : "errore");
  Serial.println(WiFi.softAPIP());
  Serial.printf("[WiFi] MAC STA: %s | MAC AP: %s\n",
                WiFi.macAddress().c_str(), WiFi.softAPmacAddress().c_str());

  if (savedSsid.length() > 0) {
    Serial.printf("[WiFi] Connessione STA salvata a: %s\n", savedSsid.c_str());
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    int retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries < 20) {
      delay(500);
      Serial.print(".");
      retries++;
    }
    Serial.println();
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] STA connesso! IP locale: http://");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("[WiFi] STA non connesso entro il timeout; AP 'TapuinoNext' attivo.");
    }
  } else {
    Serial.println("[WiFi] Nessuna rete salvata. Configura tramite AP 'TapuinoNext' su http://192.168.4.1");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifiinfo", HTTP_GET, handleWifiInfo);
  server.on("/files", HTTP_GET, handleGetFiles);
  server.on("/play", HTTP_POST, handlePlay);
  server.on("/stop", HTTP_POST, handleStop);
  server.on("/delete", HTTP_POST, handleDeleteFile);
  server.on("/rename", HTTP_POST, handleRenameFile);
  server.on("/upload", HTTP_POST, handleUploadPost, handleFileUpload);
  server.on("/ota", HTTP_POST, handleOtaPost, handleOtaUpload);
  server.on("/scan", HTTP_GET, handleScanWifi);
  server.on("/savewifi", HTTP_ANY, handleSaveWifi);
  server.on("/resetwifi", HTTP_ANY, handleResetWifi);
  server.on("/reboot", HTTP_ANY, handleReboot);
  server.begin();

  xTaskCreatePinnedToCore(webServerTask, "WebServerTask", 8192, NULL, 1, NULL, 0);
}
