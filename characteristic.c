//
// Created by martijn on 12/9/21.
//

#include "characteristic.h"

Characteristic *binc_characteristic_create(GDBusConnection *connection, const char* path) {
    Characteristic *characteristic = g_new0(Characteristic, 1);
    characteristic->connection = connection;
    characteristic->path = path;
    characteristic->uuid = NULL;
    characteristic->service_path = NULL;
    characteristic->service_uuid = NULL;
    return characteristic;
}
