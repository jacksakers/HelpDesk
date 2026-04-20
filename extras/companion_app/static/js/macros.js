// Project  : HelpDesk
// File     : js/macros.js
// Purpose  : Macro grid build and trigger
// Depends  : log.js

const MACROS = [
    { id:'mute_mic',  label:'Mute Mic',  color:'blue'    },
    { id:'skip_song', label:'Skip Song', color:'emerald' },
    { id:'macro_1',   label:'Macro 1',   color:'purple'  },
    { id:'macro_2',   label:'Macro 2',   color:'orange'  },
];

const COLORS = {
    blue:   'bg-blue-600 hover:bg-blue-500',
    emerald:'bg-emerald-600 hover:bg-emerald-500',
    purple: 'bg-purple-600 hover:bg-purple-500',
    orange: 'bg-orange-600 hover:bg-orange-500',
};

function buildMacroGrid() {
    const grid = document.getElementById('macro-grid');
    MACROS.forEach(m => {
        const btn = document.createElement('button');
        btn.className = (COLORS[m.color]||'bg-gray-600 hover:bg-gray-500')+' text-white font-medium py-2.5 px-4 rounded-lg transition text-sm';
        btn.textContent = m.label;
        btn.onclick = () => triggerMacro(m.id);
        grid.appendChild(btn);
    });
}

async function triggerMacro(macroId) {
    try { await fetch('/api/macros/'+encodeURIComponent(macroId)+'/trigger', {method:'POST'}); }
    catch { logEvent('dashboard', macroId, 'Error: request failed'); }
}
