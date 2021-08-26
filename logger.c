//
// Created by martijn on 26/8/21.
//

#include "logger.h"
#include <glib.h>

void log_debug(const char *tag, const char *message) {
    g_print("[%s] %s", tag, message);
}

void log_info(const char *tag, const char *message) {
    g_print("[%s] %s", tag, message);
}