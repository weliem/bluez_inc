//
// Created by martijn on 04-11-21.
//

#include "service_handler_manager.h"
#include "../logger.h"
#include "observation.h"
#include "../utility.h"
#include "../device.h"

#define TAG "ServiceHandlerManager"

struct binc_service_handler_manager {
    GHashTable *service_handlers;
};

ServiceHandlerManager *binc_service_handler_manager_create() {
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
            if (handler->service_handler_free != NULL) {
                handler->service_handler_free(handler);
            }
            g_free(handler);
        }
        g_hash_table_destroy(serviceHandlerManager->service_handlers);
    }
    serviceHandlerManager->service_handlers = NULL;
    g_free(serviceHandlerManager);
}


static void log_observation(Observation *observation) {
    char *time_string = binc_date_time_format_iso8601(observation->timestamp);
    log_debug(TAG, "observation{value=%.1f, unit=%s, type='%s', timestamp=%s, location=%s}",
              observation->value,
              observation_unit_str(observation->unit),
              observation_get_display_str(observation),
              time_string,
              observation_location_str(observation->location));
    g_free(time_string);
}

static gboolean g_date_time_is_later_or_equal_than(GDateTime *end, GDateTime *begin) {
    g_assert(end != NULL);
    if (begin == NULL) return TRUE;

    return g_date_time_compare(end, begin) >= 0;
}

static gboolean g_date_time_is_later_than(GDateTime *end, GDateTime *begin) {
    g_assert(end != NULL);
    if (begin == NULL) return TRUE;

    return g_date_time_compare(end, begin) > 0;
}

static void on_observation(const GList *observations, DeviceInfo *deviceInfo) {
    GDateTime *last_observation_timestamp = device_info_get_last_observation_timestamp(deviceInfo);
    GDateTime *latest_timestamp = NULL;
    GList *filtered_observations = NULL;

    for (GList *iterator = (GList *) observations; iterator; iterator = iterator->next) {
        Observation *observation = (Observation *) iterator->data;

        // Filter out the new observations and find the latest one
        if (g_date_time_is_later_than(observation->timestamp, last_observation_timestamp)) {
            filtered_observations = g_list_append(filtered_observations, observation);
            log_observation(observation);

            if (g_date_time_is_later_or_equal_than(observation->timestamp, latest_timestamp)) {
                latest_timestamp = observation->timestamp;
            }
        } else {
            log_debug(TAG, "ignoring old observation");
        }
    }

    if (g_list_length(filtered_observations) == 0) {
        g_list_free(filtered_observations);
        return;
    }

    if (latest_timestamp != NULL) {
        device_info_set_last_observation_timestamp(deviceInfo, latest_timestamp);
    }

    g_list_free(filtered_observations);
}

void binc_service_handler_manager_add(ServiceHandlerManager *serviceHandlerManager, ServiceHandler *service_handler) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_handler != NULL);
    g_assert(service_handler->uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_handler->uuid));

    service_handler->observations_callback = &on_observation;
    g_hash_table_insert(serviceHandlerManager->service_handlers, g_strdup(service_handler->uuid), service_handler);
}

ServiceHandler *
binc_service_handler_manager_get(ServiceHandlerManager *serviceHandlerManager, const char *service_uuid) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(service_uuid != NULL);
    g_assert(g_uuid_string_is_valid(service_uuid));

    return g_hash_table_lookup(serviceHandlerManager->service_handlers, service_uuid);
}

void binc_service_handler_manager_device_disconnected(ServiceHandlerManager *serviceHandlerManager, Device *device) {
    g_assert(serviceHandlerManager != NULL);
    g_assert(device != NULL);
    GList *service_handlers = g_hash_table_get_values(serviceHandlerManager->service_handlers);
    for (GList *iterator = service_handlers; iterator; iterator = iterator->next) {
        ServiceHandler *handler = (ServiceHandler *) iterator->data;
        if (handler->on_device_disconnected != NULL) {
            handler->on_device_disconnected(handler, device);
        }
    }
    g_list_free(service_handlers);
}

void binc_service_handler_send_observations(const ServiceHandler *service_handler, const Device *device,
                                                   const GList *observation_list) {
    if (service_handler->observations_callback != NULL) {
        DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
        service_handler->observations_callback(observation_list, deviceInfo);
    }
}
