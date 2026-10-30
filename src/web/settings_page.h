#pragma once

#include <pgmspace.h>

// Served at / by web_server.cpp; fetches /api/status and posts to /api/wifi, /api/settings and /api/spotify/*.
// On the setup network only the Wi-Fi section shows.
const char kSettingsPage[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Now Playing Brickman</title>
<style>
:root { --bg:#fbfaf8; --surface:#fff; --text:#1d1b19; --muted:#6b655f; --border:#e6e1db; --accent:#c8102e; --ok:#1d7a3e; }
@media (prefers-color-scheme: dark) {
  :root { --bg:#161412; --surface:#1f1c1a; --text:#f1ede8; --muted:#a39b93; --border:#34302c; --accent:#e6324f; --ok:#4cc27a; }
}
* { box-sizing:border-box; }
body { margin:0; background:var(--bg); color:var(--text); font:16px/1.5 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif; }
main { max-width:520px; margin:0 auto; padding:32px 16px 48px; }
h1 { font-size:1.5rem; margin:0; }
.sub { color:var(--muted); margin:2px 0 24px; font-size:.9rem; }
section { background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:20px; margin-bottom:16px; }
h2 { font-size:1rem; margin:0 0 4px; }
.hint { color:var(--muted); font-size:.9rem; margin:0 0 14px; }
label { display:block; font-size:.9rem; margin:12px 0 4px; }
select, input { width:100%; padding:10px 12px; font:inherit; color:var(--text); background:var(--bg); border:1px solid var(--border); border-radius:8px; }
button { margin-top:16px; padding:10px 18px; font:inherit; font-weight:600; color:#fff; background:var(--accent); border:0; border-radius:8px; cursor:pointer; }
button.secondary { color:var(--text); background:transparent; border:1px solid var(--border); }
button:disabled { opacity:.5; cursor:default; }
.msg { margin:12px 0 0; font-size:.9rem; min-height:1.2em; }
.msg.ok { color:var(--ok); }
.msg.err { color:var(--accent); }
.choices { display:flex; gap:8px; margin-top:8px; }
.choices label { flex:1; margin:0; padding:10px 12px; border:1px solid var(--border); border-radius:8px; cursor:pointer; }
.choices input { width:auto; margin-right:6px; }
ol { padding-left:1.2em; margin:8px 0 0; font-size:.9rem; }
li { margin:4px 0; }
code { display:inline-block; padding:2px 6px; border-radius:4px; background:var(--bg); border:1px solid var(--border); font-size:.85rem; word-break:break-all; }
.status { font-weight:600; }
[hidden] { display:none !important; }
</style>
</head>
<body>
<main>
  <h1 id="name">Now Playing Brickman</h1>
  <p class="sub" id="sub"></p>

  <section id="wifi">
    <h2>Wi-Fi</h2>
    <p class="hint" id="wifi-hint"></p>
    <form id="wifi-form">
      <label for="ssid">Network</label>
      <select id="ssid"><option>Looking for networks&hellip;</option></select>
      <input id="ssid-other" placeholder="Network name" hidden>
      <label for="password">Password</label>
      <input id="password" type="password" autocomplete="current-password">
      <button>Connect</button>
      <p class="msg" id="wifi-msg"></p>
    </form>
  </section>

  <div id="normal" hidden>
    <section>
      <h2>Music</h2>
      <p class="hint">Where the display gets &ldquo;now playing&rdquo; from.</p>
      <form id="source-form">
        <div class="choices">
          <label><input type="radio" name="source" value="spotify">Spotify</label>
          <label><input type="radio" name="source" value="wiim">WiiM</label>
        </div>
        <button id="source-save" hidden>Switch</button>
        <p class="msg" id="source-msg"></p>
      </form>

      <form id="wiim-form" hidden>
        <p class="hint">Shows whatever your WiiM streamer plays, from any app.</p>
        <label for="wiim">Device</label>
        <select id="wiim"><option value="">Automatic (first one found)</option></select>
        <input id="wiim-other" placeholder="IP address, e.g. 192.168.1.50" hidden>
        <button>Save</button>
        <p class="msg" id="wiim-msg"></p>
      </form>

      <div id="spotify" hidden>
        <p class="hint">Shows what your Spotify account plays. Needs Spotify Premium.</p>
        <p class="status" id="sp-status"></p>
        <form id="sp-login">
          <p class="hint" style="margin:0">Once only: make your own Spotify app for the display.</p>
          <ol>
            <li>Open <a href="https://developer.spotify.com/dashboard" target="_blank" rel="noopener">developer.spotify.com/dashboard</a>, log in and click <b>Create app</b>.</li>
            <li>Name it anything (e.g. &ldquo;Brickman&rdquo;). As <b>Redirect URI</b>, add <code id="sp-redirect"></code></li>
            <li>Tick <b>Web API</b>, agree to the terms and save.</li>
            <li>Open the app&rsquo;s settings and copy its <b>Client ID</b> here.</li>
          </ol>
          <label for="sp-client">Client ID</label>
          <input id="sp-client" autocomplete="off" spellcheck="false">
          <button>Connect Spotify</button>
          <p class="msg" id="sp-login-msg"></p>
        </form>
        <form id="sp-device-form" hidden>
          <label for="sp-device">Show music playing on</label>
          <select id="sp-device"><option value="">Any device</option></select>
          <button>Save</button>
          <button type="button" class="secondary" id="sp-signout">Disconnect Spotify</button>
          <p class="msg" id="sp-device-msg"></p>
        </form>
      </div>
    </section>

    <section>
      <h2>Clock</h2>
      <p class="hint">The screensaver's time zone.</p>
      <form id="tz-form">
        <label for="tz">Time zone</label>
        <select id="tz"><option value="">Automatic</option></select>
        <button>Save</button>
        <p class="msg" id="tz-msg"></p>
      </form>
    </section>

    <section>
      <h2>Device</h2>
      <p class="hint" id="device"></p>
      <button class="secondary" id="restart">Restart</button>
    </section>
  </div>
</main>
<script>
const $ = id => document.getElementById(id);
const OTHER = "__other__";

function option(value, text) {
  const o = document.createElement("option");
  o.value = value; o.textContent = text;
  return o;
}

async function post(url, data, msg, done) {
  const el = $(msg);
  el.className = "msg"; el.textContent = "Saving…";
  try {
    const r = await fetch(url, { method: "POST", body: new URLSearchParams(data) });
    if (!r.ok) throw new Error(await r.text());
    el.className = "msg ok"; el.textContent = done;
  } catch (e) {
    el.className = "msg err"; el.textContent = e.message || "Couldn't save";
  }
}

function toggleOther(select, input) {
  $(select).onchange = () => { $(input).hidden = $(select).value !== OTHER; };
}

async function loadNetworks(current) {
  const sel = $("ssid");
  try {
    const list = await (await fetch("/api/networks")).json();
    sel.replaceChildren();
    for (const n of list) sel.append(option(n.ssid, n.ssid + (n.secure ? "" : " (open)")));
    if (current && !list.some(n => n.ssid === current)) sel.prepend(option(current, current));
    if (current) sel.value = current;
  } catch { sel.replaceChildren(); }
  sel.append(option(OTHER, "Other…"));
  if (!sel.value) sel.value = OTHER;
  $("ssid-other").hidden = sel.value !== OTHER;
}

async function loadWiims(current) {
  const sel = $("wiim");
  try {
    const list = await (await fetch("/api/wiims")).json();
    for (const d of list) sel.append(option(d.ip, `${d.name} (${d.ip})`));
    if (current && !list.some(d => d.ip === current)) sel.append(option(current, current));
  } catch {}
  sel.append(option(OTHER, "Enter an address…"));
  sel.value = current;
}

async function loadSpotifyDevices(current) {
  const sel = $("sp-device"), msg = $("sp-device-msg");
  msg.textContent = "Getting your Spotify devices\u2026";
  try {
    const r = await fetch("/api/spotify/devices");
    if (!r.ok) throw new Error(await r.text());
    for (const name of await r.json()) sel.append(option(name, name));
    msg.textContent = "";
  } catch (e) {
    msg.className = "msg err"; msg.textContent = e.message;
  }
  if (current && ![...sel.options].some(o => o.value === current)) sel.append(option(current, current + " (not on now)"));
  sel.value = current;
}

async function loadTimezones(current, detected) {
  const sel = $("tz");
  if (detected) sel.options[0].textContent = `Automatic (${detected.replace(/_/g, " ")})`;
  const zones = await (await fetch("/api/timezones")).json();
  for (const z of zones) sel.append(option(z, z.replace(/_/g, " ")));
  sel.value = current;
}

async function main() {
  const s = await (await fetch("/api/status")).json();
  $("name").textContent = s.name;
  document.title = s.name;
  $("sub").textContent = `Version ${s.version}`;
  toggleOther("ssid", "ssid-other");
  toggleOther("wiim", "wiim-other");

  if (s.setup) {
    $("wifi-hint").textContent = s.ssid
      ? `The display can't join “${s.ssid}”. Check the password, or pick another network.`
      : "Choose the network the display should use.";
  } else {
    $("wifi-hint").textContent = `Connected to “${s.ssid}” as ${s.hostname} (${s.ip}).`;
    $("device").textContent = `Version ${s.version} · ${s.hostname} · ${s.ip}`;
    $("normal").hidden = false;
    for (const r of document.querySelectorAll("input[name=source]")) {
      r.checked = r.value === s.source;
      r.onchange = () => { $("source-save").hidden = r.value === s.source; };
    }
    $("wiim-form").hidden = s.source !== "wiim";
    $("spotify").hidden = s.source !== "spotify";
    if (s.source === "wiim") loadWiims(s.wiim_host);
    if (s.source === "spotify") {
      $("sp-redirect").textContent = s.spotify_redirect_uri;
      $("sp-client").value = s.spotify_client_id;
      $("sp-status").textContent = s.spotify_signed_in ? "Connected to Spotify" : "Not connected yet";
      $("sp-login").hidden = s.spotify_signed_in;
      $("sp-device-form").hidden = !s.spotify_signed_in;
      if (s.spotify_signed_in) loadSpotifyDevices(s.spotify_device);
    }
    loadTimezones(s.timezone, s.timezone_detected);
  }
  loadNetworks(s.ssid);

  $("wifi-form").onsubmit = e => {
    e.preventDefault();
    const ssid = $("ssid").value === OTHER ? $("ssid-other").value.trim() : $("ssid").value;
    post("/api/wifi", { ssid, password: $("password").value }, "wifi-msg",
      `Saved. The display is restarting to join “${ssid}”. ` +
      (s.setup ? "Reconnect this phone to your usual Wi-Fi." : "This page will stop responding if the network changes."));
  };
  $("source-form").onsubmit = e => {
    e.preventDefault();
    const source = document.querySelector("input[name=source]:checked").value;
    post("/api/settings", { source }, "source-msg", "Saved. The display is restarting; reload this page in a moment.");
  };
  $("sp-login").onsubmit = async e => {
    e.preventDefault();
    const msg = $("sp-login-msg");
    msg.className = "msg"; msg.textContent = "Opening Spotify\u2026";
    try {
      const r = await fetch("/api/spotify/login", { method: "POST", body: new URLSearchParams({ client_id: $("sp-client").value.trim() }) });
      if (!r.ok) throw new Error(await r.text());
      location.href = (await r.json()).url;
    } catch (e) {
      msg.className = "msg err"; msg.textContent = e.message;
    }
  };
  $("sp-device-form").onsubmit = e => {
    e.preventDefault();
    post("/api/settings", { spotify_device: $("sp-device").value }, "sp-device-msg", "Saved. The display is restarting.");
  };
  $("sp-signout").onclick = () => post("/api/spotify/signout", {}, "sp-device-msg", "Disconnected. The display is restarting.");
  $("wiim-form").onsubmit = e => {
    e.preventDefault();
    const host = $("wiim").value === OTHER ? $("wiim-other").value.trim() : $("wiim").value;
    post("/api/settings", { wiim_host: host }, "wiim-msg", "Saved. The display is restarting.");
  };
  $("tz-form").onsubmit = e => {
    e.preventDefault();
    post("/api/settings", { timezone: $("tz").value }, "tz-msg", "Saved. The display is restarting.");
  };
  $("restart").onclick = () => post("/api/restart", {}, "tz-msg", "Restarting…");
}
main();
</script>
</body>
</html>
)HTML";
