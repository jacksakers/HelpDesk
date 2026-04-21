// Project  : HelpDesk
// File     : serial_fs.h
// Purpose  : Serial-FS command dispatcher — handles fs/tasks/calendar cmds over UART
// Depends  : pc_monitor.cpp (calls serialFsDispatch from its process_line)

#pragma once

#include <ArduinoJson.h>

// Process a parsed JSON command received over the serial link.
// Returns true when the "cmd" field was recognised and handled.
// The caller (pc_monitor) should return immediately when true is returned.
bool serialFsDispatch(const JsonDocument & doc);
