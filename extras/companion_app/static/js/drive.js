// Project  : HelpDesk
// File     : js/drive.js
// Purpose  : Drive tab — SD card browser, file upload/download/delete, mkdir,
//            text editor modal, and image viewer modal
// Depends  : utils.js

let _driveCwd = '/';

function driveLoad() { driveNavigate(_driveCwd); }

function driveBreadcrumb(path) {
    const bc = document.getElementById('drive-breadcrumb');
    bc.innerHTML = '';
    const parts = path.split('/').filter(Boolean);
    const root = document.createElement('button');
    root.className = 'text-cyan-400 hover:text-cyan-300 shrink-0';
    root.textContent = '/';
    root.onclick = () => driveNavigate('/');
    bc.appendChild(root);
    let cum = '';
    parts.forEach(part => {
        cum += '/' + part;
        const sep = document.createElement('span');
        sep.textContent = '\u203a'; sep.className = 'text-gray-600 mx-1 shrink-0';
        bc.appendChild(sep);
        const p = cum;
        const btn = document.createElement('button');
        btn.className = 'text-cyan-400 hover:text-cyan-300 shrink-0 max-w-xs truncate';
        btn.textContent = part;
        btn.onclick = () => driveNavigate(p);
        bc.appendChild(btn);
    });
}

async function driveNavigate(path) {
    _driveCwd = path || '/';
    driveBreadcrumb(_driveCwd);
    const listEl = document.getElementById('drive-file-list');
    listEl.innerHTML = '<p class="text-gray-500 text-sm italic p-5">Loading\u2026</p>';
    try {
        const res = await fetch('/api/drive/list?dir=' + encodeURIComponent(_driveCwd));
        if (!res.ok) {
            const d = await res.json().catch(() => ({}));
            listEl.innerHTML = '<p class="text-red-400 text-sm p-5">Error: ' + escapeHtml(d.detail || 'Cannot list directory') + '</p>';
            return;
        }
        renderDriveEntries(await res.json());
    } catch(e) {
        listEl.innerHTML = '<p class="text-red-400 text-sm p-5">Could not reach device \u2014 check Settings \u2192 HelpDesk IP.</p>';
    }
}

const _DRIVE_ICONS = { folder:'\uD83D\uDCC1', txt:'\uD83D\uDCC4', json:'\uD83D\uDCC4', md:'\uD83D\uDCC4', jpg:'\uD83D\uDDBC\uFE0F', jpeg:'\uD83D\uDDBC\uFE0F', png:'\uD83D\uDDBC\uFE0F', bmp:'\uD83D\uDDBC\uFE0F', bin:'\uD83D\uDDBC\uFE0F', mp3:'\uD83C\uDFB5', pdf:'\uD83D\uDCD1' };
const _TEXT_EXTS = new Set(['txt','json','md']);
const _IMG_EXTS  = new Set(['jpg','jpeg','png','bmp']);

function _fileExt(name) { const p = name.split('.'); return p.length > 1 ? p.pop().toLowerCase() : ''; }

function driveIcon(name, is_dir) {
    if (is_dir) return '\uD83D\uDCC1';
    return _DRIVE_ICONS[name.split('.').pop().toLowerCase()] || '\uD83D\uDCCE';
}

function driveFmtSize(b) {
    if (b == null) return '\u2014';
    if (b < 1024) return b + ' B';
    if (b < 1048576) return (b/1024).toFixed(1) + ' KB';
    return (b/1048576).toFixed(1) + ' MB';
}

function renderDriveEntries(entries) {
    const listEl = document.getElementById('drive-file-list');
    listEl.innerHTML = '';
    if (!entries || entries.length === 0) {
        listEl.innerHTML = '<p class="text-gray-500 text-sm italic p-5">Empty folder</p>';
        return;
    }
    const sorted = [...entries].sort((a, b) => {
        if (a.is_dir !== b.is_dir) return a.is_dir ? -1 : 1;
        return a.name.localeCompare(b.name);
    });
    sorted.forEach(entry => {
        const fullPath = (_driveCwd === '/' ? '' : _driveCwd) + '/' + entry.name;
        const ext = _fileExt(entry.name);
        const isText    = !entry.is_dir && _TEXT_EXTS.has(ext);
        const isViewImg = !entry.is_dir && _IMG_EXTS.has(ext);
        const row = document.createElement('div');
        row.className = 'flex items-center gap-3 px-4 py-2.5 hover:bg-gray-700 transition group ' + (entry.is_dir || isText || isViewImg ? 'cursor-pointer' : '');
        if (entry.is_dir)   row.onclick = () => driveNavigate(fullPath);
        else if (isText)    row.onclick = () => openTextEditor(fullPath);
        else if (isViewImg) row.onclick = () => openImageViewer(fullPath);

        const icon = document.createElement('span');
        icon.textContent = driveIcon(entry.name, entry.is_dir);
        icon.className = 'text-base w-6 text-center shrink-0';

        const nameEl = document.createElement('span');
        nameEl.textContent = entry.name;
        nameEl.className = 'flex-1 text-sm text-white truncate';

        const sizeEl = document.createElement('span');
        sizeEl.textContent = entry.is_dir ? '\u2014' : driveFmtSize(entry.size);
        sizeEl.className = 'text-xs text-gray-500 shrink-0 w-20 text-right font-mono';

        const actions = document.createElement('div');
        actions.className = 'flex gap-2 opacity-0 group-hover:opacity-100 transition shrink-0';

        if (!entry.is_dir) {
            const dlBtn = document.createElement('button');
            dlBtn.textContent = '\u2B07'; dlBtn.title = 'Download';
            dlBtn.className = 'text-sm text-blue-400 hover:text-blue-300 transition';
            dlBtn.onclick = e => { e.stopPropagation(); driveDownload(fullPath, entry.name); };
            actions.appendChild(dlBtn);
        }
        const delBtn = document.createElement('button');
        delBtn.textContent = '\u2715'; delBtn.title = 'Delete';
        delBtn.className = 'text-sm text-red-400 hover:text-red-300 transition';
        delBtn.onclick = e => { e.stopPropagation(); driveDelete(fullPath, entry.is_dir); };
        actions.appendChild(delBtn);

        row.appendChild(icon); row.appendChild(nameEl); row.appendChild(sizeEl); row.appendChild(actions);
        listEl.appendChild(row);
    });
}

function driveDownload(path, filename) {
    const a = document.createElement('a');
    a.href = '/api/drive/download?path=' + encodeURIComponent(path);
    a.download = filename;
    a.click();
}

async function driveDelete(path, is_dir) {
    const kind = is_dir ? 'folder' : 'file';
    if (!confirm('Delete ' + kind + ' "' + path.split('/').pop() + '"? This cannot be undone.')) return;
    try {
        const res = await fetch('/api/drive/delete', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path}) });
        const d = await res.json().catch(() => ({}));
        if (res.ok) driveLoad();
        else driveMsgStatus('Delete failed: ' + (d.detail || d.error || 'error'), 'red');
    } catch { driveMsgStatus('Network error', 'red'); }
}

function driveDrop(e) {
    e.preventDefault();
    document.getElementById('drive-drop-zone').classList.remove('border-cyan-400');
    driveUploadFileList(e.dataTransfer.files);
}

function driveUploadFiles(e) { driveUploadFileList(e.target.files); e.target.value = ''; }

async function driveUploadFileList(files) {
    if (!files || files.length === 0) return;
    const st = document.getElementById('drive-upload-status');
    st.classList.remove('hidden');
    for (const file of files) {
        st.textContent = 'Uploading ' + file.name + '\u2026';
        st.className = 'text-xs text-gray-400';
        try {
            const form = new FormData(); form.append('file', file);
            const res = await fetch('/api/drive/upload?dir=' + encodeURIComponent(_driveCwd), { method:'POST', body:form });
            const d = await res.json().catch(() => ({}));
            st.textContent = res.ok ? ('Uploaded: ' + file.name) : ('Failed: ' + file.name + ' \u2014 ' + (d.detail || 'error'));
            st.className = 'text-xs ' + (res.ok ? 'text-emerald-400' : 'text-red-400');
        } catch { st.textContent = 'Network error uploading ' + file.name; st.className = 'text-xs text-red-400'; }
    }
    driveLoad();
    setTimeout(() => st.classList.add('hidden'), 3000);
}

function driveMsgStatus(msg, color) {
    const el = document.getElementById('drive-upload-status');
    el.textContent = msg; el.className = 'text-xs text-' + color + '-400';
    el.classList.remove('hidden');
    setTimeout(() => el.classList.add('hidden'), 3000);
}

function showMkdirDialog() {
    document.getElementById('mkdir-dialog').classList.remove('hidden');
    document.getElementById('mkdir-name').value = '';
    setTimeout(() => document.getElementById('mkdir-name').focus(), 50);
}

function hideMkdirDialog() { document.getElementById('mkdir-dialog').classList.add('hidden'); }

async function mkdirConfirm() {
    const name = document.getElementById('mkdir-name').value.trim();
    if (!name) return;
    hideMkdirDialog();
    const path = (_driveCwd === '/' ? '' : _driveCwd) + '/' + name;
    try {
        const res = await fetch('/api/drive/mkdir', { method:'POST', headers:{'Content-Type':'application/json'}, body:JSON.stringify({path}) });
        if (res.ok) driveLoad();
        else driveMsgStatus('Create folder failed', 'red');
    } catch { driveMsgStatus('Network error', 'red'); }
}

// ── Text editor modal ────────────────────────────────────────────────────────

let _editorPath = '';

async function openTextEditor(path) {
    _editorPath = path;
    document.getElementById('editor-filename').textContent = path.split('/').pop();
    const ta = document.getElementById('editor-textarea');
    ta.value = 'Loading\u2026';
    document.getElementById('editor-save-status').textContent = '';
    document.getElementById('text-editor-modal').classList.remove('hidden');
    try {
        const r = await fetch('/api/drive/download?path=' + encodeURIComponent(path));
        ta.value = r.ok ? await r.text() : '// Error loading file';
    } catch { ta.value = '// Network error'; }
    ta.focus();
}

async function editorSave() {
    const content = document.getElementById('editor-textarea').value;
    const st = document.getElementById('editor-save-status');
    st.textContent = 'Saving\u2026'; st.className = 'text-xs text-gray-400';
    try {
        const r = await fetch('/api/drive/write', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({path: _editorPath, content}),
        });
        if (r.ok) {
            st.textContent = 'Saved \u2713'; st.className = 'text-xs text-emerald-400';
            setTimeout(() => { st.textContent = ''; }, 2000);
        } else { st.textContent = 'Save failed'; st.className = 'text-xs text-red-400'; }
    } catch { st.textContent = 'Network error'; st.className = 'text-xs text-red-400'; }
}

function closeTextEditor() {
    document.getElementById('text-editor-modal').classList.add('hidden');
}

// ── Image viewer modal ───────────────────────────────────────────────────────

function openImageViewer(path) {
    document.getElementById('image-viewer-img').src =
        '/api/drive/download?path=' + encodeURIComponent(path);
    document.getElementById('image-viewer-modal').classList.remove('hidden');
}

function closeImageViewer() {
    document.getElementById('image-viewer-modal').classList.add('hidden');
    document.getElementById('image-viewer-img').src = '';
}
