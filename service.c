//
// Created by martijn on 12/9/21.
//

#include "service.h"
#include "characteristic.h"

struct binc_service {
    Device *device;
    const char *path;
    const char* uuid;
    GList *characteristics;
};

Service* binc_service_create(Device *device, const char* path, const char* uuid) {
    g_assert(device != NULL);
    g_assert(path != NULL);
    g_assert(uuid != NULL);

    Service *service = g_new0(Service, 1);
    service->device = device;
    service->path = g_strdup(path);
    service->uuid = uuid;
    service->characteristics = NULL;
    return service;
}

void binc_service_free(Service *service) {
    g_assert(service != NULL);
    g_free((char*) service->path);
    service->path = NULL;
    g_free((char*) service->uuid);
    service->uuid = NULL;
    service->device = NULL;
    g_list_free(service->characteristics);
    g_free(service);
}

const char* binc_service_get_uuid(Service *service) {
    g_assert(service != NULL);
    return service->uuid;
}

Device *binc_service_get_device(Service *service) {
    g_assert(service != NULL);
    return service->device;
}

void binc_service_add_characteristic(Service *service, Characteristic *characteristic) {
    g_assert(service != NULL);
    g_assert(characteristic != NULL);

    service->characteristics = g_list_append(service->characteristics, characteristic);
}

GList *binc_service_get_characteristics(Service *service) {
    g_assert(service != NULL);
    return service->characteristics;
}

Characteristic *binc_service_get_characteristic(Service *service, const char* char_uuid) {
    g_assert(service != NULL);
    g_assert(char_uuid != NULL);

    if (service->characteristics != NULL) {
        for (GList *iterator = service->characteristics; iterator; iterator = iterator->next) {
            Characteristic *characteristic = (Characteristic *) iterator->data;
            if (g_str_equal(char_uuid, binc_characteristic_get_uuid(characteristic))) {
                return characteristic;
            }
        }
    }
    return NULL;
}