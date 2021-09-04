//
// Created by martijn on 26/8/21.
//

#include "logger.h"
#include <glib.h>
#include <sys/time.h>

#define BUFFER_SIZE 512

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

void log_log(const char *tag, const char *level, const char *message) {
    char* timestamp = current_time_string();
    g_print("%s:%03lld %s [%s] %s\n", timestamp, current_timestamp_in_millis() % 1000, level, tag, message);
    g_free(timestamp);
}

void log_debug(const char *tag, const char *format, ...) {
    char buf[BUFFER_SIZE];
    va_list arg;
    va_start(arg, format);
    g_vsnprintf(buf, BUFFER_SIZE, format, arg);
    log_log(tag, "DEBUG", buf);
    va_end(arg);
}

void log_info(const char *tag, const char *format, ...) {
    char buf[BUFFER_SIZE];
    va_list arg;
    va_start(arg, format);
    g_vsnprintf(buf, BUFFER_SIZE, format, arg);
    log_log(tag, "INFO", buf);
    va_end(arg);
}