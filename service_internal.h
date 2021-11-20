//
// Created by martijn on 20-11-21.
//

#ifndef TEST_SERVICE_INTERNAL_H
#define TEST_SERVICE_INTERNAL_H

Service *binc_service_create(Device *device, const char *path, const char *uuid);
void binc_service_free(Service *service);
void binc_service_add_characteristic(Service *service, Characteristic *characteristic);

#endif //TEST_SERVICE_INTERNAL_H
