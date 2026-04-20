(function() {
    "use strict";
    const API_BASE = location.hostname === '192.168.4.1' ? '' : 'http://192.168.4.1';
    let isBusy = false;

    // Mapping for UI simulation
    const colorMap = { 'cyan':0x00,'lightblue':0x01,'blue':0x02,'dimpurple':0x03,'midnight':0x04,'lavender':0x05,'white':0x06,'purple':0x07,'brightpink':0x08,'lightpink':0x09,'lighterpink':0x0A,'lighterpink2':0x0B,'brightpink2':0x0C,'brightpink3':0x0D,'pinkred':0x0E,'yelloworange':0x0F,'lightyellow':0x10,'yellow':0x11,'lime':0x12,'orange':0x13,'redorange':0x14,'red':0x15,'brightcyan':0x16,'brightcyan2':0x17,'darkcyan':0x18,'green':0x19,'limegreen':0x1A,'brightlightblue':0x1B,'brightlightblue2':0x1C,'black':0x1D,'purpleish':0x1E,'random':0x1F };

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
        const patterns = { 0x1: "1 Short", 0x2: "2 Short", 0x3: "3 Short", 0x4: "1S, 2L", 0x5: "6 Short", 0x6: "S.O.S. Sequence", 0x7: "Ramping (2.0s)", 0x8: "Pulse Train (1.25s)", 0x9: "1 Medium", 0xA: "1 Long", 0xB: "2 Super Long" };
        return patterns[val] || "Standard Pulse";
    }

    function getSegmentName(raw) {
        const mask = (raw >> 5) & 0x07;
        const names = { 0: "Everything", 1: "Top-Left", 2: "Top-Right", 3: "Bottom-Left", 4: "Bottom-Right", 5: "Center (Mickey)", 6: "Outer Ring", 7: "Everything" };
        return names[mask] || "Unknown Mask";
    }

    function decode(hex) {
        const box = document.getElementById('protocolSummary');
        if (!box) return;
        const h = hex.replace(/[^A-F0-9]/gi, '').toUpperCase();
        if (h.length < 4) { box.textContent = "Waiting for data..."; return; }
        
        const idx = h.indexOf('E9');
        if (h.startsWith('CC')) { box.innerHTML = "<strong>[PING]</strong> Wake Call"; return; }
        if (idx === -1) { box.textContent = "Non-E9 Data"; return; }
        
        const act = h.substr(idx+2, 2);
        const data = h.substr(idx+4);
        const b = []; for(let i=0; i<data.length; i+=2) b.push(parseInt(data.substr(i,2),16));
        
        let msg = "";
        let warn = "";
        if (act === '06') { b.forEach((byte, i) => { if (i >= 3 && i <= 4 && ((byte >> 5) & 0x07) !== 0x02) warn = " [⚠️ Dual-mode header mismatch]"; }); }

        if (act === '05' && b.length >= 4) msg = `<strong>[SOLID]</strong> Color: <span style="color:var(--accent-2)">${getColorName(b[3])}</span> — Zone: <span style="color:var(--accent-1)">${getSegmentName(b[3])}</span>`;
        else if (act === '06' && b.length >= 5) msg = `<strong>[DUAL]</strong> <span style="color:var(--accent-2)">${getColorName(b[4])}</span> (Outer) & <span style="color:var(--accent-2)">${getColorName(b[3])}</span> (Inner)`;
        else if (act === '09' && b.length >= 8) {
            msg = `<strong>[BURST]</strong> Zones:<br>` +
                  `<span class="tiny">CN: ${getColorName(b[3])}, </span>` +
                  `<span class="tiny">TR: ${getColorName(b[4])}, </span>` +
                  `<span class="tiny">BR: ${getColorName(b[5])}, </span>` +
                  `<span class="tiny">BL: ${getColorName(b[6])}, </span>` +
                  `<span class="tiny">TL: ${getColorName(b[7])}</span>`;
        } else if (act === '0B') {
            msg = `<strong>[MOTION]</strong> Dual Green Comet Chase`;
        } else if (act === '0C' && b.length >= 6) {
            const sig = h.substr(idx+10, 6);
            const vib = h.substr(h.length - 2);
            if (sig === '5D465B') {
                msg = vib === '95' ? `<strong>[SHOW]</strong> White/Pink Sequential Fade` : `<strong>[SHOW]</strong> Full-Strand Rainbow Chase`;
            } else if (sig === '4F4F5B') {
                msg = `<strong>[SHOW]</strong> Yellow/Orange/Off Strobe Cycle`;
            } else if (sig === 'B1B9B5') {
                msg = `<strong>[SHOW]</strong> Multi-Zone Color Step`;
            } else {
                msg = `<strong>[SHOW]</strong> Complex Animation Action`;
            }
        } else msg = `<strong>[ACTION ${act}]</strong> Custom sequence`;

        const vibIdx = b[b.length-1];
        if (typeof vibIdx !== 'undefined' && (vibIdx & 0xF0) === 0xB0) msg += `<br><strong>Vibe</strong>: ${getVibrationName(vibIdx)}`;
        if (b.length >= 2) msg += ` — <em>Est. ${(1.75*(b[1]&0xF)-0.25).toFixed(1)}s</em>`;
        box.innerHTML = msg + (warn ? `<div class="tiny" style="color:#ff6b6b; margin-top:4px">${warn}</div>` : "");
    }

    function h2b() { const hv = document.getElementById('rawHex').value.replace(/[^A-F0-9]/gi,'').toUpperCase(); document.getElementById('rawBits').value = hv.split('').map(c=>parseInt(c,16).toString(2).padStart(4,'0')).join(' '); decode(hv); }
    function b2h() { const bv = document.getElementById('rawBits').value.replace(/[^01]/g,''); let hv = ""; for(let i=0; i<bv.length; i+=4) hv += parseInt(bv.substr(i,4).padEnd(4,'0'),2).toString(16).toUpperCase(); document.getElementById('rawHex').value = hv; decode(hv); }

    async function fire(body, manualHex=null) {
        if (isBusy) return;
        if (manualHex) { document.getElementById('rawHex').value = manualHex; h2b(); }
        isBusy = true;
        try {
            await fetch(`${API_BASE}/command`, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'action=ping' }).catch(()=>{});
            await new Promise(r=>setTimeout(r,400));
            await fetch(`${API_BASE}/command`, { method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body });
        } catch(e) {}
        finally { isBusy = false; }
    }

    window.addEventListener('DOMContentLoaded', () => {
        const h = document.getElementById('rawHex'); const b = document.getElementById('rawBits');
        if (h && b) { h.oninput=h2b; b.oninput=b2h; h2b(); }
    });

    window.sendPreset = (c) => {
        const v = document.getElementById('vibToggle').checked ? document.getElementById('vibPattern').value : 0;
        const hex = `8301E905002E0E${(0xE0|(colorMap[c]||0x1B)).toString(16).toUpperCase()}${(0xB0|(v&0xF)).toString(16).toUpperCase()}`;
        fire(`action=preset&color=${c}&vib=${v}`, hex);
    };
    window.sendPing = () => fire('action=ping', 'CC03000000');
    window.sendHex = () => fire(`action=hex&data=${document.getElementById('rawHex').value.replace(/\s/g,'')}`);
    window.sendManual = () => fire(document.getElementById('manual').value);

})();