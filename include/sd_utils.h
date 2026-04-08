// Project  : HelpDesk
// File     : sd_utils.h
// Purpose  : Shared SD card read/write helpers — used by http_server, settings, etc.
// Depends  : sd_card.h, <SD.h>

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Create a directory on the SD card if it does not already exist.
// Returns true on success or if the directory already exists.
bool sdEnsureDir(const char * path);

// Atomically overwrite a file with the supplied byte buffer.
// Creates the file if it does not exist; removes it first if it does.
// Returns true only if every byte was written.
bool sdWriteFile(const char * path, const uint8_t * data, size_t len);

#ifdef __cplusplus
} /* extern "C" */
#endif
