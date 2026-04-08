// Project  : HelpDesk
// File     : sd_utils.cpp
// Purpose  : Shared SD card read/write helpers
// Depends  : sd_utils.h, sd_card.h, <SD.h>

#include "sd_utils.h"
#include "sd_card.h"
#include <Arduino.h>
#include <SD.h>

bool sdEnsureDir(const char * path)
{
    if (!sdCardMounted()) return false;
    if (SD.exists(path))  return true;
    return SD.mkdir(path);
}

bool sdWriteFile(const char * path, const uint8_t * data, size_t len)
{
    if (!sdCardMounted() || !data) return false;

    if (SD.exists(path)) SD.remove(path);

    File f = SD.open(path, FILE_WRITE);
    if (!f) {
        Serial.printf("[SD] sdWriteFile: could not open %s\n", path);
        return false;
    }

    size_t written = f.write(data, len);
    f.close();

    if (written != len) {
        Serial.printf("[SD] sdWriteFile: wrote %u of %u bytes to %s\n",
                      written, len, path);
        return false;
    }
    return true;
}
