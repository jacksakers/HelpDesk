import asyncio
import json
import logging
from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import HTMLResponse
import uvicorn

# Try to import psutil for real PC stats. If not installed, we'll mock it later.
try:
    import psutil
    HAS_PSUTIL = True
except ImportError:
    HAS_PSUTIL = False
    print("WARNING: 'psutil' not found. Sending mocked telemetry data.")

# Try to import pyserial (Ready for when you actually plug in the ESP32)
try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False
    print("WARNING: 'pyserial' not found. Serial communication will be mocked.")

esp32_port = None

def auto_connect_esp32():
    """Scans COM ports to find and connect to the ESP32."""
    global esp32_port
    if not HAS_SERIAL:
        return
        
    if esp32_port is not None and esp32_port.is_open:
        return # Already connected

    ports = serial.tools.list_ports.comports()
    # Common USB-to-Serial identifiers for ESP32 boards
    esp_keywords = ["CP210", "CH34", "FT232", "USB to UART", "USB Serial", "JTAG"]
    
    for port in ports:
        if any(kw in port.description or kw in port.hwid for kw in esp_keywords):
            try:
                esp32_port = serial.Serial(port.device, 115200, timeout=1)
                logging.info(f"✅ Auto-connected to ESP32 on {port.device} ({port.description})")
                return
            except serial.SerialException as e:
                logging.error(f"Failed to open {port.device}: {e}")
                
    # Fallback: Just try to grab the first active USB serial device if keywords fail
    for port in ports:
        if "USB" in port.hwid:
            try:
                esp32_port = serial.Serial(port.device, 115200, timeout=1)
                logging.info(f"⚠️ Guessed ESP32 on {port.device} ({port.description})")
                return
            except serial.SerialException:
                pass


# Initialize FastAPI app
app = FastAPI(title="HelpDesk Companion App")
logging.basicConfig(level=logging.INFO)

# -----------------------------------------------------------------------------
# HTML Frontend (Served directly from this file for the PoC)
# In the future, you'd move this to a 'templates/index.html' file.
# -----------------------------------------------------------------------------
HTML_PAGE = """
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>HelpDesk Dashboard</title>
    <script src="https://cdn.tailwindcss.com"></script>
</head>
<body class="bg-gray-900 text-white min-h-screen p-8 font-sans">
    <div class="max-w-4xl mx-auto space-y-6">
        
        <!-- Header -->
        <div class="flex items-center justify-between bg-gray-800 p-6 rounded-xl border border-gray-700 shadow-lg">
            <div>
                <h1 class="text-3xl font-bold bg-clip-text text-transparent bg-gradient-to-r from-blue-400 to-emerald-400">HelpDesk Companion</h1>
                <p class="text-gray-400 mt-1">Host PC Management Dashboard</p>
            </div>
            <div class="flex items-center space-x-3">
                <div id="status-dot" class="w-3 h-3 rounded-full bg-red-500 animate-pulse"></div>
                <span id="status-text" class="text-sm font-medium text-gray-300">Disconnected</span>
            </div>
        </div>

        <div class="grid grid-cols-1 md:grid-cols-2 gap-6">
            
            <!-- Telemetry Card -->
            <div class="bg-gray-800 p-6 rounded-xl border border-gray-700 shadow-lg">
                <h2 class="text-xl font-semibold mb-4 flex items-center"><span class="mr-2">📊</span> Live PC Telemetry</h2>
                <div class="space-y-4">
                    <div>
                        <div class="flex justify-between text-sm mb-1">
                            <span class="text-gray-400">CPU Usage</span>
                            <span id="cpu-val" class="font-mono">0%</span>
                        </div>
                        <div class="w-full bg-gray-700 rounded-full h-2.5">
                            <div id="cpu-bar" class="bg-blue-500 h-2.5 rounded-full transition-all duration-300" style="width: 0%"></div>
                        </div>
                    </div>
                    <div>
                        <div class="flex justify-between text-sm mb-1">
                            <span class="text-gray-400">RAM Usage</span>
                            <span id="ram-val" class="font-mono">0%</span>
                        </div>
                        <div class="w-full bg-gray-700 rounded-full h-2.5">
                            <div id="ram-bar" class="bg-emerald-500 h-2.5 rounded-full transition-all duration-300" style="width: 0%"></div>
                        </div>
                    </div>
                </div>
                <p class="text-xs text-gray-500 mt-4 italic">This data is being streamed to your HelpDesk ESP32 over Serial.</p>
            </div>

            <!-- Macro Engine Testing -->
            <div class="bg-gray-800 p-6 rounded-xl border border-gray-700 shadow-lg">
                <h2 class="text-xl font-semibold mb-4 flex items-center"><span class="mr-2">⚡</span> Macro Engine</h2>
                <p class="text-sm text-gray-400 mb-4">Simulate sending a macro configuration or pressing a button from the dashboard.</p>
                
                <div class="space-y-3">
                    <button onclick="triggerMacro('mute_mic')" class="w-full bg-gray-700 hover:bg-gray-600 border border-gray-600 text-white font-medium py-2 px-4 rounded transition">
                        Simulate: Mute Mic Macro
                    </button>
                    <button onclick="triggerMacro('skip_song')" class="w-full bg-gray-700 hover:bg-gray-600 border border-gray-600 text-white font-medium py-2 px-4 rounded transition">
                        Simulate: Skip Song Macro
                    </button>
                </div>
                <div id="macro-response" class="mt-4 text-sm text-emerald-400 hidden">Macro sent successfully!</div>
            </div>
        </div>
    </div>

    <script>
        // WebSocket connection for real-time data
        let ws;
        
        function connect() {
            ws = new WebSocket(`ws://${window.location.host}/ws`);
            
            ws.onopen = () => {
                document.getElementById('status-dot').className = 'w-3 h-3 rounded-full bg-emerald-500 shadow-[0_0_10px_rgba(16,185,129,0.7)]';
                document.getElementById('status-text').innerText = 'Connected to Host App';
            };
            
            ws.onmessage = (event) => {
                const data = JSON.parse(event.data);
                if(data.type === 'telemetry') {
                    // Update CPU
                    document.getElementById('cpu-val').innerText = `${data.cpu}%`;
                    document.getElementById('cpu-bar').style.width = `${data.cpu}%`;
                    
                    // Update RAM
                    document.getElementById('ram-val').innerText = `${data.ram}%`;
                    document.getElementById('ram-bar').style.width = `${data.ram}%`;
                }
            };
            
            ws.onclose = () => {
                document.getElementById('status-dot').className = 'w-3 h-3 rounded-full bg-red-500 animate-pulse';
                document.getElementById('status-text').innerText = 'Disconnected - Retrying...';
                setTimeout(connect, 2000);
            };
        }

        // Trigger REST API
        async function triggerMacro(macroId) {
            try {
                const res = await fetch(`/api/macros/${macroId}/trigger`, { method: 'POST' });
                const data = await res.json();
                console.log(data);
                
                const responseDiv = document.getElementById('macro-response');
                responseDiv.innerText = `[Sent to ESP32]: ${data.message}`;
                responseDiv.classList.remove('hidden');
                setTimeout(() => responseDiv.classList.add('hidden'), 3000);
            } catch (err) {
                console.error(err);
            }
        }

        // Init
        connect();
    </script>
</body>
</html>
"""

# -----------------------------------------------------------------------------
# API Routes & WebSockets
# -----------------------------------------------------------------------------

@app.get("/")
async def serve_dashboard():
    """Serves the main HTML dashboard."""
    return HTMLResponse(content=HTML_PAGE)

@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """
    Handles real-time WebSocket connections from the Web Dashboard.
    Streams PC telemetry data to the browser every 1 second.
    """
    await websocket.accept()
    try:
        while True:
            # 1. Gather Telemetry
            if HAS_PSUTIL:
                cpu = psutil.cpu_percent(interval=None)
                ram = psutil.virtual_memory().percent
            else:
                # Mock data if psutil isn't installed
                import random
                cpu = round(random.uniform(10.0, 40.0), 1)
                ram = round(random.uniform(40.0, 60.0), 1)

            # 2. (Future) Send to ESP32 via Serial
            # This is where we pack it tight to save serial bandwidth
            esp32_payload = f'{{"c":{cpu},"r":{ram}}}\n'
            global esp32_port
            if HAS_SERIAL and esp32_port and esp32_port.is_open:
                try:
                    esp32_port.write(esp32_payload.encode('utf-8'))
                except serial.SerialException:
                    logging.warning("Lost connection to ESP32. Will try to reconnect...")
                    esp32_port.close()
                    esp32_port = None
            
            # 3. Send to Web Dashboard
            await websocket.send_json({
                "type": "telemetry",
                "cpu": cpu,
                "ram": ram
            })
            
            # Wait 1 second before next read
            await asyncio.sleep(1)
            
    except WebSocketDisconnect:
        logging.info("Dashboard client disconnected")

@app.post("/api/macros/{macro_id}/trigger")
async def trigger_macro(macro_id: str):
    """
    Endpoint triggered by the Web UI (or eventually by the ESP32 over serial).
    This acts as the Extensible Macro Engine entry point.
    """
    logging.info(f"Macro Triggered: {macro_id}")
    
    # 1. Execute Host PC action based on macro_id
    if macro_id == "mute_mic":
        # e.g., pyautogui.hotkey('ctrl', 'shift', 'm')
        action_taken = "Muted System Microphone"
    elif macro_id == "skip_song":
        # e.g., keyboard.send('next track')
        action_taken = "Skipped to next song"
    else:
        action_taken = "Unknown macro"

    # 2. (Future) Send confirmation or UI update state back to ESP32
    global esp32_port
    if HAS_SERIAL and esp32_port and esp32_port.is_open:
        try:
            esp32_port.write(f'{{"event":"macro_ok","id":"{macro_id}"}}\n'.encode('utf-8'))
        except serial.SerialException:
            pass

    return {
        "status": "success", 
        "macro_id": macro_id, 
        "message": action_taken
    }


# -----------------------------------------------------------------------------
# Background Tasks (Listening to the ESP32)
# -----------------------------------------------------------------------------

async def serial_listener():
    """
    This background task runs continuously, listening for incoming data 
    from the HelpDesk ESP32 (e.g., when a physical button is tapped).
    """
    logging.info("Serial Listener started")
    global esp32_port
    while True:
        if HAS_SERIAL:
            # Auto-reconnect if unplugged
            if esp32_port is None or not esp32_port.is_open:
                auto_connect_esp32()
                
            if esp32_port and esp32_port.is_open:
                try:
                    if esp32_port.in_waiting > 0:
                        line = esp32_port.readline().decode('utf-8', errors='ignore').strip()
                        if line: # Avoid empty lines
                            try:
                                data = json.loads(line)
                                if data.get("event") == "btn_press":
                                    logging.info(f"ESP32 Button Pressed! Triggering macro: {data['id']}")
                                    # Trigger your python macro logic here!
                            except json.JSONDecodeError:
                                logging.debug(f"Received non-JSON serial data: {line}")
                except serial.SerialException:
                    logging.warning("Serial read error. Device may have been unplugged.")
                    esp32_port.close()
                    esp32_port = None
            
        await asyncio.sleep(0.1) # Prevent CPU hogging in the listener loop

@app.on_event("startup")
async def startup_event():
    """Starts background tasks when the web server boots up."""
    asyncio.create_task(serial_listener())


# -----------------------------------------------------------------------------
# App Entry Point
# -----------------------------------------------------------------------------
if __name__ == "__main__":
    print("=======================================================")
    print(" Starting HelpDesk Companion App")
    print(" Dashboard will be available at: http://127.0.0.1:8000")
    print("=======================================================")
    uvicorn.run("main:app", host="127.0.0.1", port=8000, reload=True)