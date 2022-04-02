/*
 *   Copyright (c) 2022 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#include "logger.h"
#include <glib.h>
#include <sys/time.h>

#define BUFFER_SIZE 1024

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
    char* time_string = g_date_time_format(now, "%F %R:%S");
    g_date_time_unref(now);
    return time_string;
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