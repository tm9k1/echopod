#include "playback/playback_log.h"

#include <stdbool.h>
#include <stddef.h>

#include "hal/clock.h"
#include "hal/storage.h"

enum {
    LOG_CAPACITY = 2048,
    LAST_CAPACITY = 64
};

static char log_buffer[LOG_CAPACITY];
static uint32_t log_size;
static char last_status[LAST_CAPACITY];

static const char LOG_CARD_PATH[] = "/gapless.txt";
static const char LOG_INTERNAL_PATH[] = "/:internal/gapless.txt";

static void append_str(const char *str)
{
    if (str == NULL) {
        return;
    }
    while (*str != '\0' && log_size < LOG_CAPACITY - 4) {
        log_buffer[log_size++] = *str++;
    }
}

static void append_uint(uint32_t val)
{
    char buf[12];
    int pos = 0;
    if (val == 0) {
        buf[pos++] = '0';
    } else {
        while (val > 0) {
            buf[pos++] = (char)('0' + (val % 10));
            val /= 10;
        }
    }
    for (int i = pos - 1; i >= 0 && log_size < LOG_CAPACITY - 4; i--) {
        log_buffer[log_size++] = buf[i];
    }
}

static const char *filename_of(const char *path)
{
    if (path == NULL) {
        return "none";
    }
    const char *last = path;
    for (const char *p = path; *p != '\0'; p++) {
        if (*p == '/') {
            last = p + 1;
        }
    }
    return last;
}

static void string_copy(char *dest, const char *src, size_t max)
{
    size_t i = 0;
    while (src != NULL && src[i] != '\0' && i + 1 < max) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

void playback_log_event(const char *action, const char *path, const char *status)
{
    uint32_t now_s = hal_clock_milliseconds() / 1000;
    if (status != NULL && status[0] != '\0') {
        string_copy(last_status, status, sizeof last_status);
    }
    append_str("[");
    append_uint(now_s);
    append_str("s] ");
    append_str(action);
    append_str(": ");
    append_str(filename_of(path));
    if (status != NULL && status[0] != '\0') {
        append_str(" -> ");
        append_str(status);
    }
    append_str("\r\n");
    log_buffer[log_size] = '\0';
    if (log_size > LOG_CAPACITY - 256) {
        playback_log_flush();
    }
}

static void write_log(const char *path)
{
    if (log_size == 0) {
        return;
    }
    HalReserveResult res = hal_file_reserve(path, log_size);
    if (res <= HAL_RESERVE_CREATED || hal_file_grow(path, log_size) <= HAL_RESERVE_CREATED) {
        (void)hal_file_write_at(path, 0, log_buffer, log_size);
    }
}

void playback_log_flush(void)
{
    if (log_size == 0) {
        return;
    }
    write_log(LOG_CARD_PATH);
    write_log(LOG_INTERNAL_PATH);
}

const char *playback_log_last(void)
{
    return last_status[0] != '\0' ? last_status : "None";
}
