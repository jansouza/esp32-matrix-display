// esp32-matrix-display
// Copyright (c) 2026 Jan Souza
// SPDX-License-Identifier: MIT
// See LICENSE for full terms.

#ifndef WEB_PAGE_H
#define WEB_PAGE_H

const char WebPage[] = \
"<!DOCTYPE html>\n" \
"<html>\n" \
"<head>\n" \
"<title>ESP32 Matrix Display</title>\n" \
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n" \
"<style>\n" \
"  :root{\n" \
"    --bg:#16140f; --inset:#0e0d0a; --card:#211e17; --border:#3a3527;\n" \
"    --text:#f1ede3; --muted:#948d78; --accent:#ff9d2e; --accent-ink:#1a1206;\n" \
"    --ok:#5ecb7a; --err:#e3625a;\n" \
"    --font-display:'Courier New',monospace;\n" \
"    --font-body:-apple-system,'Segoe UI',Roboto,Helvetica,Arial,sans-serif;\n" \
"  }\n" \
"  @media (prefers-color-scheme: light){\n" \
"    :root{\n" \
"      --bg:#f4f1e8; --inset:#ffffff; --card:#ffffff; --border:#ddd6c2;\n" \
"      --text:#241f14; --muted:#7a7261; --accent:#d97a06; --accent-ink:#fff8ec;\n" \
"      --ok:#2f8f4e; --err:#c53a32;\n" \
"    }\n" \
"  }\n" \
"  :root[data-theme=\"dark\"]{\n" \
"    --bg:#16140f; --inset:#0e0d0a; --card:#211e17; --border:#3a3527;\n" \
"    --text:#f1ede3; --muted:#948d78; --accent:#ff9d2e; --accent-ink:#1a1206;\n" \
"    --ok:#5ecb7a; --err:#e3625a;\n" \
"  }\n" \
"  :root[data-theme=\"light\"]{\n" \
"    --bg:#f4f1e8; --inset:#ffffff; --card:#ffffff; --border:#ddd6c2;\n" \
"    --text:#241f14; --muted:#7a7261; --accent:#d97a06; --accent-ink:#fff8ec;\n" \
"    --ok:#2f8f4e; --err:#c53a32;\n" \
"  }\n" \
"  *{box-sizing:border-box;}\n" \
"  body{\n" \
"    margin:0; padding:28px 16px; background:var(--bg); color:var(--text);\n" \
"    font-family:var(--font-body);\n" \
"    display:flex; justify-content:center;\n" \
"  }\n" \
"  .wrap{width:100%; max-width:460px;}\n" \
"  .eyebrow{\n" \
"    font-family:var(--font-display); font-size:.68rem; letter-spacing:.16em;\n" \
"    text-transform:uppercase; color:var(--accent); margin:0 0 4px;\n" \
"  }\n" \
"  h1{\n" \
"    font-size:1.3rem; margin:0 0 20px; font-weight:600; letter-spacing:-.01em;\n" \
"    text-wrap:balance;\n" \
"  }\n" \
"  .card{\n" \
"    background:var(--card); border:1px solid var(--border); border-radius:14px;\n" \
"    padding:18px; margin-bottom:14px;\n" \
"  }\n" \
"  .card h2{\n" \
"    font-size:.72rem; text-transform:uppercase; letter-spacing:.1em;\n" \
"    color:var(--muted); margin:0 0 12px; font-weight:600;\n" \
"  }\n" \
"  input[type=text],input[type=number]{\n" \
"    width:100%; padding:11px 13px; border-radius:9px; border:1px solid var(--border);\n" \
"    background:var(--inset); color:var(--text); font-size:1rem; font-family:var(--font-body);\n" \
"  }\n" \
"  input[type=text]:focus,input[type=number]:focus{outline:2px solid var(--accent); outline-offset:1px; border-color:transparent;}\n" \
"  select{\n" \
"    width:100%; padding:11px 13px; border-radius:9px; border:1px solid var(--border);\n" \
"    background:var(--inset); color:var(--text); font-size:1rem; font-family:var(--font-body);\n" \
"  }\n" \
"  select:focus{outline:2px solid var(--accent); outline-offset:1px; border-color:transparent;}\n" \
"  .preview{\n" \
"    margin-top:10px; padding:12px 14px; border-radius:9px; background:var(--inset);\n" \
"    border:1px solid var(--border); font-family:'Courier New',monospace; font-size:1.05rem;\n" \
"    letter-spacing:.03em; min-height:1.4em; word-break:break-all; color:var(--accent);\n" \
"  }\n" \
"  .icons{display:flex; flex-wrap:wrap; gap:6px; margin-top:12px;}\n" \
"  .icons button{\n" \
"    border:1px solid var(--border); background:var(--inset); color:var(--text);\n" \
"    border-radius:8px; width:36px; height:36px; font-size:1rem; cursor:pointer;\n" \
"    display:flex; align-items:center; justify-content:center;\n" \
"  }\n" \
"  .icons button:hover{border-color:var(--accent);}\n" \
"  .icons button:focus-visible{outline:2px solid var(--accent); outline-offset:1px;}\n" \
"  .row{display:flex; align-items:center; gap:12px; margin-bottom:12px;}\n" \
"  .row label{flex:0 0 84px; color:var(--muted); font-size:.82rem;}\n" \
"  .row input[type=range]{\n" \
"    flex:1; accent-color:var(--accent); height:4px;\n" \
"  }\n" \
"  .row .val{\n" \
"    width:30px; text-align:right; font-variant-numeric:tabular-nums;\n" \
"    font-size:.82rem; color:var(--muted);\n" \
"  }\n" \
"  .modes{display:flex; gap:8px;}\n" \
"  .modes label{\n" \
"    flex:1; text-align:center; padding:9px; border-radius:9px; border:1px solid var(--border);\n" \
"    cursor:pointer; font-size:.82rem; color:var(--muted); background:var(--inset);\n" \
"  }\n" \
"  .modes input{position:absolute; opacity:0; width:0; height:0;}\n" \
"  .modes input:focus-visible + span{outline:2px solid var(--accent); outline-offset:2px;}\n" \
"  .modes label:has(input:checked){\n" \
"    border-color:var(--accent); background:var(--accent); color:var(--accent-ink); font-weight:600;\n" \
"  }\n" \
"  .send{\n" \
"    width:100%; padding:13px; border:none; border-radius:9px; background:var(--accent);\n" \
"    color:var(--accent-ink); font-size:1rem; font-weight:600; cursor:pointer; margin-top:2px;\n" \
"  }\n" \
"  .send:hover{filter:brightness(1.08);}\n" \
"  .send:focus-visible{outline:2px solid var(--text); outline-offset:2px;}\n" \
"  .status{\n" \
"    margin-top:10px; font-size:.82rem; text-align:center; min-height:1.2em; color:var(--ok);\n" \
"    opacity:0; transition:opacity .3s;\n" \
"  }\n" \
"  .status.show{opacity:1;}\n" \
"  .status.err{color:var(--err);}\n" \
"  .tabs{display:flex; gap:4px; margin-bottom:14px; border-bottom:1px solid var(--border);}\n" \
"  .tabs button{\n" \
"    flex:1; padding:10px; border:none; background:none; color:var(--muted);\n" \
"    font-size:.85rem; font-weight:600; cursor:pointer; border-bottom:2px solid transparent;\n" \
"    margin-bottom:-1px; font-family:var(--font-body);\n" \
"  }\n" \
"  .tabs button.active{color:var(--accent); border-bottom-color:var(--accent);}\n" \
"  .tabs button:focus-visible{outline:2px solid var(--accent); outline-offset:-2px;}\n" \
"  .panel{display:none;}\n" \
"  .panel.active{display:block;}\n" \
"  .hint{color:var(--muted); font-size:.78rem; margin:0 0 12px; line-height:1.5;}\n" \
"  .field{margin-bottom:12px;}\n" \
"  .field label{display:block; font-size:.78rem; color:var(--muted); margin-bottom:6px;}\n" \
"  .net-status{\n" \
"    display:flex; align-items:center; gap:8px; font-size:.82rem; color:var(--muted);\n" \
"    margin-bottom:14px; padding:10px 12px; background:var(--inset); border-radius:9px;\n" \
"    border:1px solid var(--border);\n" \
"  }\n" \
"  .net-status .dot{width:8px; height:8px; border-radius:50%; background:var(--ok); flex:0 0 auto;}\n" \
"  .net-status.warn .dot{background:var(--accent);}\n" \
"  .signal{display:flex; align-items:flex-end; gap:2px; height:12px; margin-left:auto;}\n" \
"  .signal span{width:4px; background:var(--border); border-radius:1px;}\n" \
"  .signal span:nth-child(1){height:25%;}\n" \
"  .signal span:nth-child(2){height:50%;}\n" \
"  .signal span:nth-child(3){height:75%;}\n" \
"  .signal span:nth-child(4){height:100%;}\n" \
"  .signal.lvl1 span:nth-child(1),\n" \
"  .signal.lvl2 span:nth-child(-n+2),\n" \
"  .signal.lvl3 span:nth-child(-n+3),\n" \
"  .signal.lvl4 span:nth-child(-n+4){background:var(--ok);}\n" \
"</style>\n" \
"</head>\n" \
"\n" \
"<body>\n" \
"<div class=\"wrap\">\n" \
"  <p class=\"eyebrow\">MD_MAX72xx &middot; 4x8x8 matrix</p>\n" \
"  <h1>Control panel</h1>\n" \
"\n" \
"  <div class=\"tabs\">\n" \
"    <button type=\"button\" class=\"active\" id=\"tabBtnSettings\" onclick=\"showTab('settings')\">Settings</button>\n" \
"    <button type=\"button\" id=\"tabBtnNet\" onclick=\"showTab('net')\">Network</button>\n" \
"    <button type=\"button\" id=\"tabBtnApi\" onclick=\"showTab('api')\">API</button>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel active\" id=\"panelSettings\">\n" \
"    <div class=\"card\">\n" \
"      <h2>Mode</h2>\n" \
"      <div class=\"modes\">\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"0\" checked onchange=\"onAppModeChange()\"><span>Message</span></label>\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"1\" onchange=\"onAppModeChange()\"><span>Clock</span></label>\n" \
"        <label><input type=\"radio\" name=\"appmode\" value=\"2\" onchange=\"onAppModeChange()\"><span>Game of Life</span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class=\"card\" id=\"msgCard\">\n" \
"      <h2>Message</h2>\n" \
"      <form id=\"txt_form\" onsubmit=\"return false;\">\n" \
"        <input type=\"text\" id=\"Message\" maxlength=\"255\" placeholder=\"Type your message...\">\n" \
"      </form>\n" \
"      <div class=\"preview\" id=\"preview\">&nbsp;</div>\n" \
"      <div class=\"icons\">\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[heart]')\">&hearts;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[wifi]')\">&#128225;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[smile]')\">&#128578;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[star]')\">&#9733;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[music]')\">&#9834;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[bell]')\">&#128276;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[clock]')\">&#128340;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[ok]')\">&#10003;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[x]')\">&#10007;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[sun]')\">&#9728;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[rain]')\">&#127783;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[pin]')\">&#128205;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[plus]')\">&#10133;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[warn]')\">&#9888;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[bolt]')\">&#9889;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[fire]')\">&#128293;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[up]')\">&uarr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[down]')\">&darr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[left]')\">&larr;</button>\n" \
"        <button type=\"button\" onclick=\"InsertIcon('[right]')\">&rarr;</button>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <div class=\"card\">\n" \
"      <h2>Display</h2>\n" \
"      <div class=\"row\" id=\"spdRow\">\n" \
"        <label>Speed</label>\n" \
"        <input type=\"range\" id=\"spd\" min=\"20\" max=\"250\" value=\"75\" oninput=\"onSpdInput()\">\n" \
"        <span class=\"val\" id=\"spdVal\">75</span>\n" \
"      </div>\n" \
"      <div class=\"row\">\n" \
"        <label>Brightness</label>\n" \
"        <input type=\"range\" id=\"brt\" min=\"0\" max=\"15\" value=\"8\" oninput=\"onBrtInput()\">\n" \
"        <span class=\"val\" id=\"brtVal\">8</span>\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"tzRow\" style=\"display:none;\">\n" \
"        <label>Timezone</label>\n" \
"        <select id=\"tz\" onchange=\"SendText()\"></select>\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"dateRow\" style=\"display:none;\">\n" \
"        <label style=\"flex:1 1 auto;\">Show date</label>\n" \
"        <input type=\"checkbox\" id=\"dateOn\" checked onchange=\"onDateToggle()\" style=\"width:20px; height:20px; flex:0 0 auto;\">\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"dateIvRow\" style=\"display:none;\">\n" \
"        <label>Date every (s)</label>\n" \
"        <input type=\"number\" id=\"dateIv\" min=\"5\" max=\"3600\" step=\"5\" value=\"30\" onchange=\"SendText()\">\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"dateFmtRow\" style=\"display:none;\">\n" \
"        <label>Date format</label>\n" \
"        <select id=\"dateFmt\" onchange=\"SendText()\">\n" \
"          <option value=\"br\">Day / Month</option>\n" \
"          <option value=\"us\">Month / Day</option>\n" \
"        </select>\n" \
"      </div>\n" \
"      <div class=\"row\" id=\"langRow\" style=\"display:none;\">\n" \
"        <label>Date lang</label>\n" \
"        <select id=\"lang\" onchange=\"SendText()\">\n" \
"          <option value=\"en\">English</option>\n" \
"          <option value=\"pt\">Portuguese</option>\n" \
"          <option value=\"de\">German</option>\n" \
"          <option value=\"es\">Spanish</option>\n" \
"          <option value=\"fr\">French</option>\n" \
"          <option value=\"it\">Italian</option>\n" \
"        </select>\n" \
"      </div>\n" \
"      <div class=\"modes\" id=\"msgModes\">\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"0\" checked onchange=\"SendText()\"><span>Scroll</span></label>\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"1\" onchange=\"SendText()\"><span>Blink</span></label>\n" \
"        <label><input type=\"radio\" name=\"msgmode\" value=\"2\" onchange=\"SendText()\"><span>Blink+Scroll</span></label>\n" \
"      </div>\n" \
"    </div>\n" \
"\n" \
"    <button class=\"send\" onclick=\"SendText()\">Send</button>\n" \
"    <div class=\"status\" id=\"status\">&nbsp;</div>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel\" id=\"panelNet\">\n" \
"    <div class=\"net-status\" id=\"netStatus\">\n" \
"      <span class=\"dot\"></span>\n" \
"      <span id=\"netStatusText\">Checking...</span>\n" \
"      <span class=\"signal\" id=\"signalBars\"></span>\n" \
"    </div>\n" \
"    <div class=\"card\">\n" \
"      <h2>WiFi network</h2>\n" \
"      <p class=\"hint\">Enter the name (SSID) and password of the network the display should use. Saving reboots the device and it will try to connect. If it fails, it falls back to the <strong>MD-Display-Setup</strong> network so you can try again.</p>\n" \
"      <div class=\"field\">\n" \
"        <label for=\"nssid\">Network name (SSID)</label>\n" \
"        <input type=\"text\" id=\"nssid\" maxlength=\"63\" placeholder=\"My WiFi network\">\n" \
"      </div>\n" \
"      <div class=\"field\">\n" \
"        <label for=\"npass\">Password</label>\n" \
"        <input type=\"text\" id=\"npass\" maxlength=\"63\" placeholder=\"Leave blank to keep current password\">\n" \
"      </div>\n" \
"      <button class=\"send\" onclick=\"SaveNetwork()\">Save and reboot</button>\n" \
"      <div class=\"status\" id=\"netSaveStatus\">&nbsp;</div>\n" \
"    </div>\n" \
"  </div>\n" \
"\n" \
"  <div class=\"panel\" id=\"panelApi\">\n" \
"    <div class=\"card\">\n" \
"      <h2>REST API</h2>\n" \
"      <p class=\"hint\">Set the display message remotely (Home Assistant, Node-RED, curl, etc) with:</p>\n" \
"      <div class=\"preview\" id=\"apiExample\" style=\"word-break:break-all; font-size:.85rem;\">GET /api/display?msg=Hello</div>\n" \
"      <div class=\"row\" style=\"margin-top:14px;\">\n" \
"        <label style=\"flex:1 1 auto;\">Require API key</label>\n" \
"        <input type=\"checkbox\" id=\"apiAuthToggle\" onchange=\"onApiAuthToggle()\" style=\"width:20px; height:20px; flex:0 0 auto;\">\n" \
"      </div>\n" \
"      <div class=\"field\" id=\"apiKeyField\" style=\"display:none;\">\n" \
"        <label for=\"apiKeyValue\">API key (send as header <code>X-API-Key</code>)</label>\n" \
"        <input type=\"text\" id=\"apiKeyValue\" readonly onclick=\"this.select()\">\n" \
"        <button class=\"send\" style=\"margin-top:8px;\" onclick=\"RegenerateApiKey()\">Generate new key</button>\n" \
"        <p class=\"hint\" style=\"margin-top:8px;\">Generating a new key immediately invalidates the old one - update any automations using it.</p>\n" \
"      </div>\n" \
"      <div class=\"status\" id=\"apiStatus\">&nbsp;</div>\n" \
"    </div>\n" \
"  </div>\n" \
"</div>\n" \
"\n" \
"<script>\n" \
"var iconPreview = {\n" \
"  \"[heart]\":\"\\u2764\", \"[wifi]\":\"\\u{1F4E1}\", \"[smile]\":\"\\u{1F642}\",\n" \
"  \"[star]\":\"\\u2b50\", \"[music]\":\"\\u266a\", \"[bell]\":\"\\u{1F514}\", \"[clock]\":\"\\u{1F550}\",\n" \
"  \"[ok]\":\"\\u2713\", \"[x]\":\"\\u2717\", \"[sun]\":\"\\u2600\", \"[rain]\":\"\\u{1F327}\",\n" \
"  \"[pin]\":\"\\u{1F4CD}\", \"[plus]\":\"\\u2795\",\n" \
"  \"[warn]\":\"\\u26a0\", \"[bolt]\":\"\\u26a1\", \"[fire]\":\"\\u{1F525}\",\n" \
"  \"[up]\":\"\\u2191\", \"[down]\":\"\\u2193\", \"[left]\":\"\\u2190\", \"[right]\":\"\\u2192\"\n" \
"};\n" \
"\n" \
"function InsertIcon(tag){\n" \
"  var f = document.getElementById(\"Message\");\n" \
"  f.value += tag;\n" \
"  f.focus();\n" \
"  updatePreview();\n" \
"}\n" \
"\n" \
"function updatePreview(){\n" \
"  var txt = document.getElementById(\"Message\").value;\n" \
"  var out = txt.replace(/\\[[a-z]+\\]/g, function(m){ return iconPreview[m] || m; });\n" \
"  document.getElementById(\"preview\").textContent = out || \" \";\n" \
"}\n" \
"\n" \
"function onSpdInput(){\n" \
"  var v = document.getElementById(\"spd\").value;\n" \
"  document.getElementById(\"spdVal\").textContent = v;\n" \
"}\n" \
"function onBrtInput(){\n" \
"  var v = document.getElementById(\"brt\").value;\n" \
"  document.getElementById(\"brtVal\").textContent = v;\n" \
"}\n" \
"\n" \
"function currentMsgMode(){\n" \
"  var r = document.getElementsByName(\"msgmode\");\n" \
"  for (var i=0;i<r.length;i++) if (r[i].checked) return r[i].value;\n" \
"  return \"0\";\n" \
"}\n" \
"\n" \
"function currentAppMode(){\n" \
"  var r = document.getElementsByName(\"appmode\");\n" \
"  for (var i=0;i<r.length;i++) if (r[i].checked) return r[i].value;\n" \
"  return \"0\";\n" \
"}\n" \
"\n" \
"function updateModeUI(){\n" \
"  var mode = currentAppMode();\n" \
"  var clock = mode === \"1\";\n" \
"  var life = mode === \"2\";\n" \
"  var msgMode = !clock && !life;\n" \
"  var dateOn = document.getElementById(\"dateOn\").checked;\n" \
"  document.getElementById(\"msgCard\").style.display = msgMode ? \"block\" : \"none\";\n" \
"  document.getElementById(\"spdRow\").style.display = msgMode ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"msgModes\").style.display = msgMode ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"tzRow\").style.display = clock ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"dateRow\").style.display = clock ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"langRow\").style.display = (clock && dateOn) ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"dateIvRow\").style.display = (clock && dateOn) ? \"flex\" : \"none\";\n" \
"  document.getElementById(\"dateFmtRow\").style.display = (clock && dateOn) ? \"flex\" : \"none\";\n" \
"}\n" \
"\n" \
"function onAppModeChange(){\n" \
"  updateModeUI();\n" \
"  SendText();\n" \
"}\n" \
"\n" \
"function onDateToggle(){\n" \
"  updateModeUI();\n" \
"  SendText();\n" \
"}\n" \
"\n" \
"function showStatus(msg, isError){\n" \
"  var s = document.getElementById(\"status\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 2000);\n" \
"}\n" \
"\n" \
"function SendText(){\n" \
"  var msg = document.getElementById(\"Message\").value;\n" \
"  var spd = document.getElementById(\"spd\").value;\n" \
"  var brt = document.getElementById(\"brt\").value;\n" \
"  var mode = currentMsgMode();\n" \
"  var dateIv = parseInt(document.getElementById(\"dateIv\").value, 10) || 30;\n" \
"  var qs = \"?msg=\" + encodeURIComponent(msg) +\n" \
"           \"&spd=\" + spd +\n" \
"           \"&brt=\" + brt +\n" \
"           \"&mode=\" + mode +\n" \
"           \"&dmode=\" + currentAppMode() +\n" \
"           \"&tzname=\" + encodeURIComponent(document.getElementById(\"tz\").value) +\n" \
"           \"&lang=\" + document.getElementById(\"lang\").value +\n" \
"           \"&dateon=\" + (document.getElementById(\"dateOn\").checked ? \"1\" : \"0\") +\n" \
"           \"&dateiv=\" + dateIv +\n" \
"           \"&dateus=\" + (document.getElementById(\"dateFmt\").value === \"us\" ? \"1\" : \"0\") +\n" \
"           \"&nocache=\" + Math.random();\n" \
"\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"POST\", \"/api/settings\" + qs, true);\n" \
"  request.onload = function(){\n" \
"    showStatus(\"Sent!\");\n" \
"  };\n" \
"  request.onerror = function(){ showStatus(\"Failed to send\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function showTab(name){\n" \
"  var panels = { settings: \"panelSettings\", net: \"panelNet\", api: \"panelApi\" };\n" \
"  var buttons = { settings: \"tabBtnSettings\", net: \"tabBtnNet\", api: \"tabBtnApi\" };\n" \
"  for (var key in panels){\n" \
"    document.getElementById(panels[key]).classList.toggle(\"active\", key === name);\n" \
"    document.getElementById(buttons[key]).classList.toggle(\"active\", key === name);\n" \
"  }\n" \
"}\n" \
"\n" \
"function SaveNetwork(){\n" \
"  var nssid = document.getElementById(\"nssid\").value;\n" \
"  var npass = document.getElementById(\"npass\").value;\n" \
"  if (!nssid){\n" \
"    showNetSaveStatus(\"Enter the network name\", true);\n" \
"    return;\n" \
"  }\n" \
"  var qs = \"?ssid=\" + encodeURIComponent(nssid) +\n" \
"           \"&pass=\" + encodeURIComponent(npass) +\n" \
"           \"&nocache=\" + Math.random();\n" \
"\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/network\" + qs, true);\n" \
"  request.onload = function(){ showNetSaveStatus(\"Saved! Rebooting...\"); };\n" \
"  request.onerror = function(){ showNetSaveStatus(\"Failed to save\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function showNetSaveStatus(msg, isError){\n" \
"  var s = document.getElementById(\"netSaveStatus\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 3000);\n" \
"}\n" \
"\n" \
"function showApiStatus(msg, isError){\n" \
"  var s = document.getElementById(\"apiStatus\");\n" \
"  s.textContent = msg;\n" \
"  s.classList.toggle(\"err\", !!isError);\n" \
"  s.classList.add(\"show\");\n" \
"  setTimeout(function(){ s.classList.remove(\"show\"); }, 3000);\n" \
"}\n" \
"\n" \
"function renderApiSettings(enabled, key){\n" \
"  document.getElementById(\"apiAuthToggle\").checked = enabled;\n" \
"  document.getElementById(\"apiKeyField\").style.display = enabled ? \"block\" : \"none\";\n" \
"  document.getElementById(\"apiKeyValue\").value = key || \"\";\n" \
"  document.getElementById(\"apiExample\").textContent =\n" \
"    \"GET /api/display?msg=Hello\" + (enabled ? \"  (header X-API-Key required)\" : \"\");\n" \
"}\n" \
"\n" \
"function loadApiSettings(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/apisettings?nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    if (request.responseText){\n" \
"      var data = JSON.parse(request.responseText);\n" \
"      renderApiSettings(!!data.enabled, data.key || \"\");\n" \
"    }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function onApiAuthToggle(){\n" \
"  var enabled = document.getElementById(\"apiAuthToggle\").checked;\n" \
"  var qs = \"?enabled=\" + (enabled ? \"1\" : \"0\") + \"&nocache=\" + Math.random();\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/apiauth\" + qs, true);\n" \
"  request.onload = function(){\n" \
"    showApiStatus(enabled ? \"API key required\" : \"API key not required\");\n" \
"    loadApiSettings();\n" \
"  };\n" \
"  request.onerror = function(){ showApiStatus(\"Failed to save\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function RegenerateApiKey(){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/apiregen?nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    showApiStatus(\"New key generated\");\n" \
"    loadApiSettings();\n" \
"  };\n" \
"  request.onerror = function(){ showApiStatus(\"Failed to generate key\", true); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function checkNetMode(){\n" \
"  var el = document.getElementById(\"netStatus\");\n" \
"  var txt = document.getElementById(\"netStatusText\");\n" \
"  if (location.hostname === \"192.168.4.1\"){\n" \
"    el.classList.add(\"warn\");\n" \
"    txt.textContent = \"Setup mode (AP) - enter your network below\";\n" \
"  } else {\n" \
"    txt.textContent = \"Connected\";\n" \
"  }\n" \
"}\n" \
"\n" \
"function renderSignal(rssi){\n" \
"  var el = document.getElementById(\"signalBars\");\n" \
"  el.innerHTML = \"\";\n" \
"  for (var i = 0; i < 4; i++) el.appendChild(document.createElement(\"span\"));\n" \
"  el.className = \"signal\";\n" \
"  if (!rssi) return;\n" \
"  var lvl = rssi >= -55 ? 4 : rssi >= -65 ? 3 : rssi >= -75 ? 2 : 1;\n" \
"  el.classList.add(\"lvl\" + lvl);\n" \
"  el.title = rssi + \" dBm\";\n" \
"}\n" \
"\n" \
"function loadNetInfo(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/netinfo?nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    if (request.responseText){\n" \
"      var data = JSON.parse(request.responseText);\n" \
"      document.getElementById(\"nssid\").value = data.ssid || \"\";\n" \
"      renderSignal(data.rssi || 0);\n" \
"    }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function loadTimezones(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/timezones?nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    if (request.responseText){\n" \
"      var zones = JSON.parse(request.responseText);\n" \
"      var sel = document.getElementById(\"tz\");\n" \
"      sel.innerHTML = \"\";\n" \
"      for (var i = 0; i < zones.length; i++){\n" \
"        var opt = document.createElement(\"option\");\n" \
"        opt.value = zones[i];\n" \
"        opt.textContent = zones[i];\n" \
"        sel.appendChild(opt);\n" \
"      }\n" \
"    }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"function loadCurrentSettings(next){\n" \
"  var request = new XMLHttpRequest();\n" \
"  request.open(\"GET\", \"/api/settings?nocache=\" + Math.random(), true);\n" \
"  request.onload = function(){\n" \
"    var data = JSON.parse(request.responseText);\n" \
"\n" \
"    document.getElementById(\"spd\").value = data.spd;\n" \
"    document.getElementById(\"spdVal\").textContent = data.spd;\n" \
"    document.getElementById(\"brt\").value = data.brt;\n" \
"    document.getElementById(\"brtVal\").textContent = data.brt;\n" \
"\n" \
"    var msgModeInput = document.querySelector(\"input[name=msgmode][value='\" + data.mode + \"']\");\n" \
"    if (msgModeInput) msgModeInput.checked = true;\n" \
"\n" \
"    var appModeInput = document.querySelector(\"input[name=appmode][value='\" + data.appMode + \"']\");\n" \
"    if (appModeInput) appModeInput.checked = true;\n" \
"    document.getElementById(\"tz\").value = data.tzname;\n" \
"    document.getElementById(\"lang\").value = data.lang;\n" \
"    document.getElementById(\"dateOn\").checked = data.dateon == 1;\n" \
"    document.getElementById(\"dateIv\").value = data.dateiv || 30;\n" \
"    document.getElementById(\"dateFmt\").value = data.dateus == 1 ? \"us\" : \"br\";\n" \
"    updateModeUI();\n" \
"\n" \
"    // Prefill the message box with the last message sent, unless the\n" \
"    // user already started typing.\n" \
"    var msgEl = document.getElementById(\"Message\");\n" \
"    if (data.lastmsg && !msgEl.value){ msgEl.value = data.lastmsg; updatePreview(); }\n" \
"    if (next) next();\n" \
"  };\n" \
"  request.onerror = function(){ if (next) next(); };\n" \
"  request.send(null);\n" \
"}\n" \
"\n" \
"document.getElementById(\"Message\").addEventListener(\"input\", updatePreview);\n" \
"updatePreview();\n" \
"checkNetMode();\n" \
"// Run these one at a time - the device only handles one HTTP\n" \
"// connection at a time, so firing them all in parallel makes the\n" \
"// browser queue up requests and can trip its connection timeout.\n" \
"loadTimezones(function(){\n" \
"  loadNetInfo(function(){\n" \
"    loadCurrentSettings(function(){\n" \
"      loadApiSettings();\n" \
"    });\n" \
"  });\n" \
"});\n" \
"</script>\n" \
"</body>\n" \
"</html>\n" \
"\n";

#endif // WEB_PAGE_H
