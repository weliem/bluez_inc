//
// Created by martijn on 04-11-21.
//

#include "service_handler_manager.h"

struct binc_service_handler_manager {
    GHashTable *service_handlers;
};

ServiceHandlerManager* binc_service_handler_manager_create() {
    ServiceHandlerManager *serviceHandlerManager = g_new0(ServiceHandlerManager, 1);
    serviceHandlerManager->service_handlers = g_hash_table_new(g_str_hash, g_str_equal);
    return serviceHandlerManager;
}

void binc_service_handler_manager_add(ServiceHandlerManager *serviceHandlerManager, ServiceHandler *service_handler) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_handler != NULL);
    g_assert(service_handler->uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_handler->uuid));

    g_hash_table_insert(serviceHandlerManager->service_handlers, g_strdup(service_handler->uuid), service_handler);
}

ServiceHandler* binc_service_handler_manager_get(ServiceHandlerManager *serviceHandlerManager, const char* service_uuid) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    return g_hash_table_lookup(serviceHandlerManager->service_handlers, service_uuid);
}