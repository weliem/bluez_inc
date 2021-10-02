//
// Created by martijn on 12/9/21.
//

#include "service.h"

Service* binc_service_create(const char* path, const char* uuid) {
    g_assert(path != NULL);
    g_assert(uuid != NULL);

    Service *result = g_new0(Service, 1);
    result->path = path;
    result->uuid = uuid;
    return result;
}

void binc_service_free(Service *service) {
    g_free((char*) service->path);
    g_free((char*) service->uuid);
    g_free(service);
}