//
// Created by martijn on 26/8/21.
//

#include "logger.h"
#include <glib.h>
#include <sys/time.h>

/**
 * Get the current UTC time in milliseconds since epoch
 * @return
 */
long long current_timestamp_in_millis() {
    struct timeval te;
    gettimeofday(&te, NULL); // get current time
    long long milliseconds = te.tv_sec*1000LL + te.tv_usec/1000; // calculate milliseconds
    return milliseconds;
}

/**
 * Returns a string representation of the current time year-month-day hours:minutes:seconds
 * @return newly allocated string, must be freed using g_free()
 */
char* current_time_string() {
    GDateTime *now = g_date_time_new_now_local();
    return g_date_time_format(now, "%F %R:%S");
}

void log_debug(const char *tag, const char *message) {
    char* timestamp = current_time_string();
    g_print("%s:%lld [%s] %s\n", timestamp, current_timestamp_in_millis() % 1000, tag, message);
    g_free(timestamp);
}

void log_info(const char *tag, const char *message) {
    g_print("[%s] %s\n", tag, message);
}