//
// Created by martijn on 12/9/21.
//

#ifndef TEST_CHARACTERISTIC_H
#define TEST_CHARACTERISTIC_H

#include <gio/gio.h>
#include "service.h"

typedef struct {
    GDBusConnection *connection;
    const char *path;
    const char *uuid;
    const char *service_path;
    const char *service_uuid;
    GList *flags;
} Characteristic;

Characteristic *binc_characteristic_create(GDBusConnection *connection, const char* path);
GByteArray *binc_characteristic_read(Characteristic *characteristic);

#endif //TEST_CHARACTERISTIC_H
