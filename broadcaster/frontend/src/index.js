// Import CSS
import './index.css';

const API_BASE = location.hostname === '192.168.4.1' ? '' : 'http://192.168.4.1';

/* ---------- Request state management ---------- */
let isRequestInProgress = false;

function setButtonsDisabled(disabled) {
  const buttons = document.querySelectorAll('button');
  buttons.forEach(btn => {
    btn.disabled = disabled;
    if (disabled) {
      btn.style.opacity = '0.5';
      btn.style.cursor = 'not-allowed';
    } else {
      btn.style.opacity = '1';
      btn.style.cursor = 'pointer';
    }
  });
  isRequestInProgress = disabled;
}

/* ---------- small UI helpers ---------- */
function toast(msg, ok=true){
  const el = document.createElement('div');
  el.className='toast';
  el.textContent=msg;
  el.style.background = ok ? 'linear-gradient(135deg,#22c55e,#66d39a)' : 'linear-gradient(135deg,#ff6b6b,#ff8b6b)';
  el.style.color = '#021014';
  document.body.appendChild(el);
  setTimeout(()=>el.remove(), 1400);
}

/* palette mapping */
function hexToClosestPalette(hex){
  const colors = {
    'cyan': 0x00, 'purple': 0x01, 'blue': 0x02, 'brightpurple': 0x05,
    'pink': 0x08, 'yelloworange': 0x0F, 'lime': 0x12, 'orange': 0x13,
    'red': 0x15, 'green': 0x19, 'white': 0x1B
  };
  const r = parseInt(hex.substr(1,2),16);
  const g = parseInt(hex.substr(3,2),16);
  const b = parseInt(hex.substr(5,2),16);
  if(r>200 && g<100 && b<100) return colors.red;
  if(r<100 && g<100 && b>200) return colors.blue;
  if(r<100 && g>200 && b<100) return colors.green;
  if(r>200 && g>100 && b<100) return colors.orange;
  if(r>150 && g<100 && b>150) return colors.purple;
  if(r>200 && g>100 && b>150) return colors.pink;
  if(r<100 && g>200 && b>200) return colors.cyan;
  if(r>200 && g>200 && b<100) return colors.yelloworange;
  if(r>200 && g>200 && b>200) return colors.white;
  return colors.white;
}

/* send wrapper: wakes device then sends real command */
async function sendWithWakeup(body){
  if (isRequestInProgress) {
    toast('Request in progress...', false);
    return;
  }
  
  setButtonsDisabled(true);
  
  try {
    await fetch(`${API_BASE}/command`, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'action=ping' }).catch(()=>{});
    await new Promise(r=>setTimeout(r,420));
    const res = await fetch(`${API_BASE}/command`, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
    if(!res.ok) throw new Error('request failed');
    const text = await res.text();
    toast('Sent', true);
    return text;
  } catch(e){
    toast(`Error ${e}`, false);
    throw e;
  } finally {
    setButtonsDisabled(false);
  }
}

/* Presets */
window.sendPreset = function(color){
  const vibOn = document.getElementById('vibToggle').checked;
  const vib = vibOn ? document.getElementById('vibPattern').value : 0;
  const body = `action=preset&color=${encodeURIComponent(color)}&vib=${vib}`;
  sendWithWakeup(body).catch(()=>{});
}

/* Dual color */
window.sendDual = function(){
  const c1 = hexToClosestPalette(document.getElementById('dualInner').value);
  const c2 = hexToClosestPalette(document.getElementById('dualOuter').value);
  const vibOn = document.getElementById('vibToggle').checked;
  const vib = vibOn ? document.getElementById('vibPattern').value : 0;
  const body = `action=dual&c1=${c1}&c2=${c2}&vib=${vib}`;
  sendWithWakeup(body).catch(()=>{});
}

/* Crossfade */
window.sendCross = function(){
  const c1 = hexToClosestPalette(document.getElementById('crossA').value);
  const c2 = hexToClosestPalette(document.getElementById('crossB').value);
  const vibOn = document.getElementById('vibToggle').checked;
  const vib = vibOn ? document.getElementById('vibPattern').value : 0;
  const body = `action=crossfade&c1=${c1}&c2=${c2}&vib=${vib}`;
  sendWithWakeup(body).catch(()=>{});
}

/* Rainbow */
window.sendRainbow = function(){
  const c1 = hexToClosestPalette(document.getElementById('r1').value);
  const c2 = hexToClosestPalette(document.getElementById('r2').value);
  const c3 = hexToClosestPalette(document.getElementById('r3').value);
  const c4 = hexToClosestPalette(document.getElementById('r4').value);
  const c5 = hexToClosestPalette(document.getElementById('r5').value);
  const vibOn = document.getElementById('vibToggle').checked;
  const vib = vibOn ? document.getElementById('vibPattern').value : 0;
  const body = `action=rainbow&c1=${c1}&c2=${c2}&c3=${c3}&c4=${c4}&c5=${c5}&vib=${vib}`;
  sendWithWakeup(body).catch(()=>{});
}

/* Circle */
window.sendCircle = function(){
  const vibOn = document.getElementById('vibToggle').checked;
  const vib = vibOn ? document.getElementById('vibPattern').value : 0;
  const body = `action=circle&vib=${vib}`;
  sendWithWakeup(body).catch(()=>{});
}

/* Ping/wake */
window.sendPing = function(){
  if (isRequestInProgress) {
    toast('Request in progress...', false);
    return;
  }
  
  setButtonsDisabled(true);
  
  fetch(`${API_BASE}/command`, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'action=ping' })
    .then(r=>{ 
      if(r.ok) toast('Wake sent', true); 
      else toast('Wake failed', false); 
    })
    .catch(()=>toast('Wake failed', false))
    .finally(()=>setButtonsDisabled(false));
}

/* Manual */
window.sendManual = function(){
  const txt = document.getElementById('manual').value.trim();
  if(!txt){ toast('Enter command', false); return; }
  sendWithWakeup(txt).catch(()=>{});
}

/* Raw Hex Broadcast */
window.sendHex = function(){
  const hexInput = document.getElementById('rawHex');
  const hex = hexInput.value.trim().replace(/\s/g, '');
  if(!hex){ toast('Enter hex string', false); return; }
  if(hex.length % 2 !== 0){ toast('Hex must be even length', false); return; }
  if(!/^[0-9A-Fa-f]+$/.test(hex)){ toast('Invalid hex characters', false); return; }
  
  const body = `action=hex&data=${encodeURIComponent(hex)}`;
  sendWithWakeup(body).catch(()=>{});
}

/* ---------- Live Protocol Interpreter ---------- */
function getColorName(raw) {
    const index = raw & 0x1F;
    const names = {
        0x00: "cyan", 0x01: "light blue", 0x02: "blue", 0x03: "dim purple",
        0x04: "midnight blue", 0x05: "bright lavender", 0x06: "white", 0x07: "purple",
        0x08: "bright pink", 0x09: "light pink", 0x0A: "lighter pink", 0x0B: "lighter pink 2",
        0x0C: "bright pink 2", 0x0D: "bright pink 3", 0x0E: "pink/red", 0x0F: "yellow orange",
        0x10: "light yellow", 0x11: "yellow", 0x12: "lime", 0x13: "orange",
        0x14: "red orange", 0x15: "red", 0x16: "bright cyan", 0x17: "bright cyan 2",
        0x18: "dark cyan", 0x19: "green", 0x1A: "lime green", 0x1B: "bright light blue",
        0x1C: "bright light blue 2", 0x1D: "black", 0x1E: "purple-ish", 0x1F: "random"
    };
    return names[index] || `Index ${index.toString(16).toUpperCase()}`;
}

function getVibrationName(raw) {
    const val = raw & 0x0F;
    if (val === 0) return "Off";
    const patterns = {
        0x1: "1 Short", 0x2: "2 Short", 0x3: "3 Short",
        0x4: "1S, 2L", 0x5: "6 Short", 0x6: "S.O.S. Sequence",
        0x7: "Ramping (2.0s)", 0x8: "Pulse Train (1.25s)",
        0x9: "1 Medium", 0xA: "1 Long", 0xB: "2 Super Long"
    };
    return patterns[val] || "Standard Pulse";
}

function getSegmentName(raw) {
    const mask = (raw >> 5) & 0x07;
    const names = {
        0: "Everything", 1: "Top-Left", 2: "Top-Right", 
        3: "Bottom-Left", 4: "Bottom-Right", 5: "Center (Mickey)",
        6: "Outer Ring", 7: "Everything"
    };
    return names[mask] || "Unknown Mask";
}

function updateProtocolDescription(hex) {
    const summary = document.getElementById('protocolSummary');
    if (!summary) return;
    
    const h = hex.replace(/[^0-9A-Fa-f]/g, '').toUpperCase();
    if (h.length < 4) { summary.textContent = "Waiting for data..."; return; }
    
    const e9Idx = h.indexOf('E9');
    if (h.startsWith('CC')) {
        summary.innerHTML = "<strong>[PING]</strong> Broadcaster System Wake";
        return;
    }
    
    if (e9Idx === -1) { summary.textContent = "Unknown Protocol (Non-E9)"; return; }
    
    const action = h.substr(e9Idx + 2, 2);
    const data = h.substr(e9Idx + 4);
    const b = []; for(let i=0; i<data.length; i+=2) b.push(parseInt(data.substr(i,2), 16));

    let desc = "";
    let warn = "";

    // Dual Mode Header check (Expecting 010b in bits 5-7)
    if (action === '06') {
        b.forEach((byte, i) => {
            if (i >= 3 && i <= 4) { // Color bytes
                if (((byte >> 5) & 0x07) !== 0x02) warn = " [⚠️ Dual-mode header mismatch]";
            }
        });
    }

    if (action === '05' && b.length >= 4) {
        desc = `<strong>[SOLID]</strong> Color: <span style="color:var(--accent-2)">${getColorName(b[3])}</span> — Zone: <span style="color:var(--accent-1)">${getSegmentName(b[3])}</span>`;
    } else if (action === '06' && b.length >= 5) {
        desc = `<strong>[DUAL]</strong> <span style="color:var(--accent-2)">${getColorName(b[4])}</span> (Outer) & <span style="color:var(--accent-2)">${getColorName(b[3])}</span> (Inner)`;
    } else if (action === '09' && b.length >= 8) {
        desc = `<strong>[BURST]</strong> Zones:<br>` +
               `<span class="tiny">CN: ${getColorName(b[3])}, </span>` +
               `<span class="tiny">TR: ${getColorName(b[4])}, </span>` +
               `<span class="tiny">BR: ${getColorName(b[5])}, </span>` +
               `<span class="tiny">BL: ${getColorName(b[6])}, </span>` +
               `<span class="tiny">TL: ${getColorName(b[7])}</span>`;
    } else if (action === '0B') {
        desc = `<strong>[MOTION]</strong> Dual Green Comet Chase`;
    } else if (action === '0C' && b.length >= 6) {
        const sig = h.substr(e9Idx + 10, 6);
        const vib = h.substr(h.length - 2);
        if (sig === '5D465B') {
            desc = vib === '95' ? `<strong>[SHOW]</strong> White/Pink Sequential Fade` : `<strong>[SHOW]</strong> Full-Strand Rainbow Chase`;
        } else if (sig === '4F4F5B') {
            desc = `<strong>[SHOW]</strong> Yellow/Orange/Off Strobe Cycle`;
        } else if (sig === 'B1B9B5') {
            desc = `<strong>[SHOW]</strong> Multi-Zone Color Step`;
        } else {
            desc = `<strong>[SHOW]</strong> Complex Animation Action`;
        }
    } else if (action === '11') {
        desc = `<strong>[FADE]</strong> Palette Cross-fade`;
    } else {
        desc = `<strong>[ACTION ${action}]</strong> Undocumented command.`;
    }

    // Vibration parsing
    const vibByte = b[b.length-1];
    if (typeof vibByte !== 'undefined' && (vibByte & 0xF0) === 0xB0) {
        desc += `<br><strong>Vibe</strong>: ${getVibrationName(vibByte)}`;
    }

    // Duration hint
    if (b.length >= 2) {
        const val = b[1] & 0x0F;
        const secs = (1.75 * val - 0.25).toFixed(1);
        desc += ` — <em>Est. ${secs}s duration</em>`;
    }

    summary.innerHTML = desc + (warn ? `<div class="tiny" style="color:#ff6b6b; margin-top:4px">${warn}</div>` : "");
}

/* ---------- Bi-directional Sync Logic ---------- */
function syncHexToBits() {
  const h = document.getElementById('rawHex');
  const b = document.getElementById('rawBits');
  if (!h || !b) return;
  let hex = h.value.replace(/[^0-9A-Fa-f]/g, '').toUpperCase();
  let bits = hex.split('').map(char => parseInt(char, 16).toString(2).padStart(4, '0')).join(' ');
  b.value = bits;
  updateProtocolDescription(hex);
}

function syncBitsToHex() {
  const h = document.getElementById('rawHex');
  const b = document.getElementById('rawBits');
  if (!h || !b) return;
  let bits = b.value.replace(/[^01]/g, '');
  let hex = "";
  for (let i = 0; i < bits.length; i += 4) {
    let chunk = bits.substr(i, 4);
    if (chunk.length < 4) chunk = chunk.padEnd(4, '0');
    hex += parseInt(chunk, 2).toString(16).toUpperCase();
  }
  h.value = hex;
  updateProtocolDescription(hex);
}

// Ensure listeners are attached after DOM is ready
window.addEventListener('DOMContentLoaded', () => {
  const h = document.getElementById('rawHex');
  const b = document.getElementById('rawBits');
  if (h && b) {
    h.addEventListener('input', syncHexToBits);
    b.addEventListener('input', syncBitsToHex);
    syncHexToBits(); // Initial sync
    console.log("MagicBand Bit-Sync Logic: Online");
  }
});