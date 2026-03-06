#pragma once

#include <cstdint>

/**
 * SD Card Firmware Updater
 *
 * Handles firmware updates from SD card files.
 * Supports /update/firmware.bin file on boot.
 *
 * Usage:
 *   - Call sd_update_check() in setup() to check for and perform updates
 *   - Returns true if update was performed and device will reboot
 *   - Returns false if no update needed
 */

// Check for and perform firmware update from SD card
// Returns true if update was performed (device will reboot)
// Returns false if no update or error
bool sd_update_check();

// Manual update - flash a specific file (returns true if successful)
// File path should start with "/" (e.g., "/update/firmware.bin")
// Returns true if flash was successful
bool sd_update_file(const char* file_path);

// Get the last update status message
// Useful for GUI integration
const char* sd_update_get_status();
