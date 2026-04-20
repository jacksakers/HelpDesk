// Project  : HelpDesk
// File     : js/media.js
// Purpose  : Image and audio upload to the HelpDesk SD card

const _files = {};

function handleDrop(e, type) {
    e.preventDefault();
    document.getElementById(type+'-drop-zone').classList.remove(type==='image'?'border-blue-400':'border-emerald-400');
    const file = e.dataTransfer.files[0];
    if (file) _setFile(type, file);
}

function handleFileSelect(e, type) { const f=e.target.files[0]; if(f) _setFile(type,f); }

function _setFile(type, file) {
    _files[type] = file;
    document.getElementById(type+'-filename').textContent = file.name;
    document.getElementById(type+'-preview').classList.remove('hidden');
    document.getElementById(type+'-status').classList.add('hidden');
}

async function uploadFile(type) {
    const file = _files[type]; if (!file) return;
    const endpoint = type==='image' ? '/api/media/image' : '/api/media/audio';
    const statusEl = document.getElementById(type+'-status');
    statusEl.textContent='Uploading...'; statusEl.className='mt-2 text-xs text-gray-400'; statusEl.classList.remove('hidden');
    const body = new FormData(); body.append('file', file);
    try {
        const res  = await fetch(endpoint, {method:'POST', body});
        const data = await res.json();
        if (res.ok) {
            const dest = data.sent_to_device ? 'Saved and sent to device' : 'Saved locally (device not connected)';
            statusEl.textContent = 'OK: '+data.filename+' -- '+dest;
            statusEl.className   = 'mt-2 text-xs text-emerald-400';
        } else {
            statusEl.textContent = 'Error: '+(data.detail||'Upload failed');
            statusEl.className   = 'mt-2 text-xs text-red-400';
        }
    } catch { statusEl.textContent='Network error'; statusEl.className='mt-2 text-xs text-red-400'; }
}
