// Project  : HelpDesk
// File     : js/websocket.js
// Purpose  : WebSocket connection, connection indicators, and device state updates
// Depends  : log.js, chat.js

let ws;

function connectWs() {
    ws = new WebSocket('ws://'+window.location.host+'/ws');
    ws.onopen    = ()=>setIndicator('ws',true,'Dashboard');
    ws.onmessage = ({data})=>{
        const msg=JSON.parse(data);
        if(msg.type==='telemetry')    { document.getElementById('cpu-val').innerText=msg.cpu+'%'; document.getElementById('cpu-bar').style.width=msg.cpu+'%'; document.getElementById('ram-val').innerText=msg.ram+'%'; document.getElementById('ram-bar').style.width=msg.ram+'%'; }
        if(msg.type==='macro_event')  logEvent(msg.source, msg.macro_id, msg.action);
        if(msg.type==='device_info')   updateDeviceInfo(msg);
        if(msg.type==='device_status') updateDeviceStatus(msg);
        if(msg.type==='serial_status') setIndicator('serial',msg.connected,'HelpDesk');
        if(msg.type==='serial_log')    logSerialEvent(msg.label||'');
        if(msg.type==='lora_msg')      chatReceive(msg);
        if(msg.type==='lora_raw')      chatReceiveRaw(msg);
    };
    ws.onclose = ()=>{ setIndicator('ws',false,'Dashboard'); setIndicator('serial',false,'HelpDesk'); setTimeout(connectWs,2000); };
}

function setIndicator(id, connected, label) {
    document.getElementById(id+'-dot').className = connected ? 'w-2.5 h-2.5 rounded-full bg-emerald-500' : 'w-2.5 h-2.5 rounded-full bg-red-500 animate-pulse';
    document.getElementById(id+'-text').innerText = connected ? label+': Connected' : label+': Disconnected';
}

function updateDeviceInfo(d) {
    document.getElementById('device-card').classList.remove('hidden');
    const link = document.getElementById('device-ip-link');
    link.textContent = d.ip || '--';
    link.href        = d.ip ? 'http://'+d.ip+'/status' : '#';
    document.getElementById('device-ssid').textContent = d.ssid  || '--';
    document.getElementById('device-fw').textContent   = d.fw    || '--';
    if (d.sd_ok && d.sd_total_mb > 0) {
        const pct = Math.round(d.sd_used_mb / d.sd_total_mb * 100);
        document.getElementById('device-sd-val').textContent =
            d.sd_used_mb+' MB / '+d.sd_total_mb+' MB ('+pct+'%)';
        document.getElementById('device-sd-bar').style.width = pct+'%';
        document.getElementById('device-sd-bar').className =
            'h-2 rounded-full transition-all duration-500 '+(pct > 80 ? 'bg-red-500' : 'bg-purple-500');
    }
    const txt = document.getElementById('serial-text');
    if (d.ip && d.ip !== '0.0.0.0') {
        txt.innerHTML = 'HelpDesk: <span class="font-mono text-blue-300">'+d.ip+'</span>';
    }
}

function updateDeviceStatus(d) {
    document.getElementById('device-card').classList.remove('hidden');
    if (d.screen) document.getElementById('device-screen').textContent = d.screen;
    const valEl = document.getElementById('device-sd-val');
    const match = valEl.textContent.match(/\/ (\d+) MB/);
    if (match && d.sd_used_mb !== undefined) {
        const total = parseInt(match[1]);
        const pct   = Math.round(d.sd_used_mb / total * 100);
        valEl.textContent = d.sd_used_mb+' MB / '+total+' MB ('+pct+'%)';
        document.getElementById('device-sd-bar').style.width = pct+'%';
    }
}
