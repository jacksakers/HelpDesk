// Project  : HelpDesk
// File     : js/settings.js
// Purpose  : Settings form — load from device and save

async function loadSettings() {
    try {
        const data = await (await fetch('/api/settings')).json();
        const form = document.getElementById('settings-form');
        for (const [k, v] of Object.entries(data)) { const el=form.elements[k]; if(el) el.value=v; }
    } catch (err) { console.error('Failed to load settings:', err); }
}

async function saveSettings(e) {
    e.preventDefault();
    const form=document.getElementById('settings-form'), statusEl=document.getElementById('settings-status'), payload={};
    for (const el of form.elements) { if(el.name) payload[el.name]=el.value; }
    try {
        const res = await fetch('/api/settings', {method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify(payload)});
        statusEl.textContent = res.ok ? 'Settings saved' : 'Save failed';
        statusEl.className   = 'text-sm '+(res.ok?'text-emerald-400':'text-red-400');
        statusEl.classList.remove('hidden');
        setTimeout(()=>statusEl.classList.add('hidden'), 3000);
    } catch { console.error('Settings save failed'); }
}

function togglePwd(inputId, btn) {
    const input=document.getElementById(inputId), hidden=input.type==='password';
    input.type=hidden?'text':'password'; btn.textContent=hidden?'Hide':'Show';
}
