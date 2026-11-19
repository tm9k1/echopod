#ifndef PLAYBACK_PLAYBACK_LOG_H
#define PLAYBACK_PLAYBACK_LOG_H

#include <stdint.h>

void playback_log_event(const char *action, const char *path, const char *status);
void playback_log_flush(void);
const char *playback_log_last(void);

#endif
