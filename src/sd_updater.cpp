#include "sd_updater.h"
#include <Update.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include "esp_ota_ops.h"

// SD card should be mounted via mount_filesystem() before calling this
extern FILE* file_open(const char* path, const char* mode);

// Status buffer for GUI/debug messages
static char _status_msg[256] = "";

const char* sd_update_get_status()
{
    return _status_msg;
}

// Helper to get file size
static size_t get_file_size(FILE* f)
{
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    size_t size = ftell(f);
    fseek(f, 0, SEEK_SET);
    return size;
}

// Flash firmware from file
// Returns true if successful
static bool flash_firmware(const char* filepath)
{
    printf("sd_update: Opening %s...\n", filepath);

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: Cannot open %s", filepath);
        printf("sd_update: %s\n", _status_msg);
        return false;
    }

    // Get file size
    size_t file_size = get_file_size(f);
    printf("sd_update: File size: %zu bytes\n", file_size);

    if (file_size == 0) {
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: File is empty");
        printf("sd_update: %s\n", _status_msg);
        fclose(f);
        return false;
    }

    // Get flash size from chip
    // Default partition expects firmware to fit in available space
    // Typical: factory partition is ~1.9 MB for 4MB flash
    if (file_size > 2048 * 1024) {  // 2MB limit (safe for most partitions)
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: File too large (%zu bytes > 2MB)", file_size);
        printf("sd_update: %s\n", _status_msg);
        fclose(f);
        return false;
    }

    // Begin OTA update (despite the name, this works for direct flash too)
    if (!Update.begin(file_size)) {
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: Cannot begin update (flash space?)");
        printf("sd_update: %s\n", _status_msg);
        printf("sd_update: Update error code: %d\n", Update.getError());
        fclose(f);
        return false;
    }

    // Flash the file in 4KB chunks
    const size_t CHUNK_SIZE = 4096;
    uint8_t buf[CHUNK_SIZE];
    size_t bytes_flashed = 0;

    while (bytes_flashed < file_size) {
        size_t to_read = (file_size - bytes_flashed) > CHUNK_SIZE ? CHUNK_SIZE : (file_size - bytes_flashed);
        size_t n = fread(buf, 1, to_read, f);

        if (n != to_read) {
            snprintf(_status_msg, sizeof(_status_msg), "ERROR: Read failed at %zu bytes", bytes_flashed);
            printf("sd_update: %s\n", _status_msg);
            Update.abort();
            fclose(f);
            return false;
        }

        if (Update.write(buf, n) != n) {
            snprintf(_status_msg, sizeof(_status_msg), "ERROR: Flash write failed at %zu bytes", bytes_flashed);
            printf("sd_update: %s\n", _status_msg);
            printf("sd_update: Update error code: %d\n", Update.getError());
            Update.abort();
            fclose(f);
            return false;
        }

        bytes_flashed += n;
        printf("sd_update: Flashed %zu / %zu bytes (%.1f%%)\n",
               bytes_flashed, file_size, (bytes_flashed * 100.0) / file_size);
    }

    // size_t bytes_flashed = 0;
    // uint8_t buf[1];

    // while (bytes_flashed < file_size) {
    //     size_t n = fread(buf, 1, 1, f);
    //     if (n != 1) {
    //         snprintf(_status_msg, sizeof(_status_msg), "ERROR: Read failed at %zu bytes", bytes_flashed);
    //         printf("sd_update: %s\n", _status_msg);
    //         Update.abort();
    //         fclose(f);
    //         return false;
    //     }

    //     if (Update.write(buf, n) != n) {
    //         snprintf(_status_msg, sizeof(_status_msg), "ERROR: Flash write failed at %zu bytes", bytes_flashed);
    //         printf("sd_update: %s\n", _status_msg);
    //         printf("sd_update: Update error code: %d\n", Update.getError());
    //         Update.abort();
    //         fclose(f);
    //         return false;
    //     }

    //     bytes_flashed += n;
    //     printf("sd_update: Flashed %zu / %zu bytes (%.1f%%)\n",
    //            bytes_flashed, file_size, (bytes_flashed * 100.0) / file_size);
    // }

    // Finalize the update
    if (!Update.end()) {
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: Update finalization failed");
        printf("sd_update: %s\n", _status_msg);
        printf("sd_update: Update error code: %d\n", Update.getError());
        fclose(f);
        return false;
    }

    fclose(f);

    snprintf(_status_msg, sizeof(_status_msg), "SUCCESS: Flashed %zu bytes - rebooting...", bytes_flashed);
    printf("sd_update: %s\n", _status_msg);

    return true;
}

bool sd_update_file(const char* file_path)
{
    if (!file_path) return false;

    printf("sd_update: Attempting manual update from %s\n", file_path);

    // Check if file exists
    FILE* f = fopen(file_path, "rb");
    if (!f) {
        snprintf(_status_msg, sizeof(_status_msg), "ERROR: File not found: %s", file_path);
        printf("sd_update: %s\n", _status_msg);
        return false;
    }
    fclose(f);

    return flash_firmware(file_path);
}

bool sd_update_check()
{
    const char* update_file = "/update/firmware.bin";

    printf("sd_update: Checking for %s on boot...\n", update_file);

    // Check if update file exists
    FILE* f = fopen(update_file, "rb");
    if (!f) {
        printf("sd_update: No update file found - continuing normal boot\n");
        esp_ota_mark_app_valid_cancel_rollback();
        return false;
    }
    fclose(f);

    printf("sd_update: Found %s - starting update process\n", update_file);

    // Flash the firmware
    if (!flash_firmware(update_file)) {
        return false;
    }

    // Delete the update file
    printf("sd_update: Deleting %s after successful flash\n", update_file);
    if (remove(update_file) != 0) {
        printf("sd_update: WARNING - could not delete update file\n");
    }

    // Device will reboot in a moment due to OTA finalization
    // Give user a brief moment to see the success message
    printf("sd_update: Rebooting in 3 seconds...\n");
    vTaskDelay(3000 / portTICK_PERIOD_MS);

    // Force reboot
    esp_restart();
    return true;  // Never reached but makes compiler happy
}
