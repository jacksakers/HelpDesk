// Project  : HelpDesk
// File     : js/log.js
// Purpose  : Activity log — event and serial log entries
// Depends  : utils.js

function logEvent(source, id, action) {
    const log = document.getElementById('activity-log');
    const empty = document.getElementById('log-empty');
    if (empty) empty.remove();
    const time   = new Date().toLocaleTimeString();
    const label  = source==='device' ? 'HelpDesk' : 'Dashboard';
    const border = source==='device' ? 'border-orange-400' : 'border-blue-400';
    const color  = source==='device' ? 'text-orange-300'   : 'text-blue-300';
    const entry = document.createElement('div');
    entry.className = 'border-l-2 pl-2 py-0.5 '+border;
    entry.innerHTML = '<span class="text-gray-500 text-xs">'+time+'</span> <span class="'+color+'">'+label+'</span> -> <span class="text-white">'+id+'</span> <span class="text-gray-400 text-xs">-- '+action+'</span>';
    log.prepend(entry);
    while (log.children.length > 50) log.removeChild(log.lastChild);
}

function logSerialEvent(label) {
    const log = document.getElementById('activity-log');
    const empty = document.getElementById('log-empty');
    if (empty) empty.remove();
    const time = new Date().toLocaleTimeString();
    const entry = document.createElement('div');
    entry.className = 'border-l-2 pl-2 py-0.5 border-yellow-700';
    entry.innerHTML = '<span class="text-gray-500 text-xs">'+time+'</span> <span class="text-yellow-400">HelpDesk</span> <span class="text-gray-300 text-xs font-mono">'+escapeHtml(label)+'</span>';
    log.prepend(entry);
    while (log.children.length > 50) log.removeChild(log.lastChild);
}

function clearLog() {
    document.getElementById('activity-log').innerHTML = '<p id="log-empty" class="text-gray-500 text-xs italic">No events yet.</p>';
}
