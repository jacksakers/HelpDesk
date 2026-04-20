// Project  : HelpDesk
// File     : js/chat.js
// Purpose  : DeskChat — LoRa group chat receive and send
// Depends  : utils.js

const MAX_CHAT_BUBBLES = 200;
let _chatBubbleCount = 0;

function _chatAppend(html) {
    const log = document.getElementById('chat-log');
    if (!log) return;
    if (_chatBubbleCount === 0) log.innerHTML = '';
    const div = document.createElement('div');
    div.innerHTML = html;
    log.appendChild(div.firstChild);
    _chatBubbleCount++;
    while (_chatBubbleCount > MAX_CHAT_BUBBLES && log.firstChild) {
        log.removeChild(log.firstChild);
        _chatBubbleCount--;
    }
    log.scrollTop = log.scrollHeight;
}

function chatReceive(msg) {
    const user = msg.user || 'Anon';
    const id   = msg.id   || '?';
    const text = msg.msg  || '';
    const rssi = msg.rssi ? ` <span class="text-gray-600">${msg.rssi} dBm</span>` : '';
    const isSelf = msg.self === true;
    const nameColor = isSelf ? 'text-emerald-400' : 'text-blue-400';
    _chatAppend(`<div class="py-1 border-b border-gray-800">
        <span class="${nameColor} font-bold">${escHtml(user)}</span>
        <span class="text-gray-600 text-xs ml-1">[${escHtml(id)}]</span>${rssi}
        <p class="text-gray-200 mt-0.5 whitespace-pre-wrap">${escHtml(text)}</p>
    </div>`);
    const btn = document.getElementById('tab-btn-chat');
    if (btn && document.getElementById('tab-chat').classList.contains('hidden')) {
        btn.classList.add('text-emerald-400');
    }
}

function chatReceiveRaw(msg) {
    const raw  = msg.raw  || '';
    const rssi = msg.rssi ? ` ${msg.rssi} dBm` : '';
    _chatAppend(`<div class="py-1 border-b border-gray-800">
        <span class="text-yellow-600 text-xs">[observe]${rssi}</span>
        <p class="text-gray-500 font-mono text-xs mt-0.5 break-all">${escHtml(raw)}</p>
    </div>`);
}

async function chatSend() {
    const input = document.getElementById('chat-input');
    const msg = (input.value || '').trim();
    if (!msg) return;
    input.value = '';
    try {
        const r = await fetch('/api/chat/send', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({msg})
        });
        if (!r.ok) {
            _chatAppend(`<div class="text-red-400 text-xs py-1">Send failed: ${r.status}</div>`);
        }
    } catch(e) {
        _chatAppend(`<div class="text-red-400 text-xs py-1">Network error: ${e.message}</div>`);
    }
}
