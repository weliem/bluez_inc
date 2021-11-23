//
// Created by martijn on 04-11-21.
//

#include "service_handler_manager.h"
#include "../logger.h"
#include "observation.h"
#include "../utility.h"
#include "../fhir_uploader.h"

#define TAG "ServiceHandlerManager"

struct binc_service_handler_manager {
    GHashTable *service_handlers;
};

ServiceHandlerManager* binc_service_handler_manager_create() {
    ServiceHandlerManager *serviceHandlerManager = g_new0(ServiceHandlerManager, 1);
    serviceHandlerManager->service_handlers = g_hash_table_new(g_str_hash, g_str_equal);
    return serviceHandlerManager;
}

void binc_service_handler_manager_free(ServiceHandlerManager *serviceHandlerManager) {
    if (serviceHandlerManager->service_handlers != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, serviceHandlerManager->service_handlers);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            ServiceHandler *handler = (ServiceHandler *) value;
            handler->service_handler_free(handler);
            g_free(handler);
        }
        g_hash_table_destroy(serviceHandlerManager->service_handlers);
    }
    serviceHandlerManager->service_handlers = NULL;
    g_free(serviceHandlerManager);
}

static void on_observation(GList *observations) {
    for (GList *iterator = observations; iterator; iterator = iterator->next) {
        Observation *observation = (Observation *) iterator->data;

        char* time_string = binc_date_time_format_iso8601(observation->timestamp);
        log_debug(TAG, "observation{value=%.1f, unit=%s, type='%s', timestamp=%s, location=%s}",
                  observation->value,
                  observation_unit_str(observation->unit),
                  observation_get_display_str(observation),
                  time_string,
                  observation_location_str(observation->location));
        g_free(time_string);
    }

    char* fhir = observation_list_as_fhir(observations);
    //g_print("%s", fhir);
    postFhir(fhir);
    g_free(fhir);
}

void binc_service_handler_manager_add(ServiceHandlerManager *serviceHandlerManager, ServiceHandler *service_handler) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_handler != NULL);
    g_assert(service_handler->uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_handler->uuid));

    service_handler->observations_callback = &on_observation;
    g_hash_table_insert(serviceHandlerManager->service_handlers, g_strdup(service_handler->uuid), service_handler);
}

ServiceHandler* binc_service_handler_manager_get(ServiceHandlerManager *serviceHandlerManager, const char* service_uuid) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    return g_hash_table_lookup(serviceHandlerManager->service_handlers, service_uuid);
}