//
// Created by martijn on 12/9/21.
//

#ifndef TEST_SERVICE_H
#define TEST_SERVICE_H

#include <gio/gio.h>
#include "forward_decl.h"


Service *binc_service_create(Device *device, const char *path, const char *uuid);

void binc_service_free(Service *service);

const char *binc_service_get_uuid(Service *service);

Device *binc_service_get_device(Service *service);

void binc_service_add_characteristic(Service *service, Characteristic *characteristic);

GList *binc_service_get_characteristics(Service *service);

Characteristic *binc_service_get_characteristic(Service *service, const char *char_uuid);

#endif //TEST_SERVICE_H
