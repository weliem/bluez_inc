//
// Created by martijn on 12-01-22.
//

#include "plx_service_handler.h"
#include "plx_features.h"
#include "../../characteristic.h"
#include "../../parser.h"
#include "../../logger.h"
#include "../../device.h"
#include "../observation.h"
#include "plx_spot_measurement.h"

#define TAG "PLX_Service_Handler"

typedef struct plx_internal {
    GHashTable *features;
} PlxInternal;

static void plx_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));

    binc_device_read_char(device, PLX_SERVICE_UUID, PLX_FEATURE_CHAR_UUID);
    binc_device_start_notify(device, PLX_SERVICE_UUID, PLX_SPOT_MEASUREMENT_CHAR_UUID);
}

static void plx_onNotificationStateUpdated(ServiceHandler *service_handler,
                                           Device *device,
                                           Characteristic *characteristic,
                                           const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    log_debug(TAG, "characteristic <%s> notifying %s", uuid, is_notifying ? "true" : "false");
}



static void handleFeature(const ServiceHandler *service_handler, const Device *device, const GByteArray *byteArray) {
    PlxFeatures *plxFeatures = plx_create_features(byteArray);
    g_assert(plxFeatures != NULL);

    PlxInternal *plxInternal = (PlxInternal *) service_handler->private_data;
    g_assert(plxInternal != NULL);

    g_hash_table_insert(plxInternal->features, g_strdup(binc_device_get_address(device)), plxFeatures);
    log_debug(TAG, "got PLX feature");
}

static void handleSpotMeasurement(const ServiceHandler *service_handler, const Device *device,
                                  const GByteArray *byteArray) {

    SpotMeasurement *measurement = plx_create_spot_measurement(byteArray);

    if (measurement->timestamp == NULL) {
        log_debug(TAG, "Ignoring spot measurement without timestamp");
        return;
    }

    // Make sure we only send valid spot measurements
    if (measurement->measurement_status != NULL) {
        PlxInternal *plxInternal = (PlxInternal *) service_handler->private_data;
        PlxFeatures *plxFeatures = g_hash_table_lookup(plxInternal->features, binc_device_get_address(device));
        if (plxFeatures != NULL && plxFeatures->measurementStatusSupport->isFullyQualifiedDataSupported) {
            if (!measurement->measurement_status->isFullyQualifiedData) {
                log_debug(TAG, "Ignoring spot measurement because it is not a valid measurement");
                return;
            }
        }
    }

    GList *observation_list = plx_spot_measurement_as_observations(measurement);
    plx_spot_measurement_free(measurement);

    if (service_handler->observations_callback != NULL) {
        DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
        service_handler->observations_callback(observation_list, deviceInfo);
    }
    observation_list_free(observation_list);
}

static void plx_onCharacteristicChanged(ServiceHandler *service_handler,
                                        Device *device,
                                        Characteristic *characteristic,
                                        const GByteArray *byteArray,
                                        const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    g_str_equal(uuid, PLX_FEATURE_CHAR_UUID) ? handleFeature(service_handler, device, byteArray) :
    g_str_equal(uuid, PLX_SPOT_MEASUREMENT_CHAR_UUID) ? handleSpotMeasurement(service_handler, device, byteArray)
                                                      : NULL;
}

static void plx_free(ServiceHandler *service_handler) {
    PlxInternal *plxInternal = (PlxInternal *) service_handler->private_data;
    g_assert(plxInternal != NULL);

    g_hash_table_destroy(plxInternal->features);
    g_free(plxInternal);
    log_debug(TAG, "freeing PLX private_data");
}

static void plx_onDeviceDisconnected(ServiceHandler *service_handler, Device *device) {
    PlxInternal *plxInternal = (PlxInternal *) service_handler->private_data;
    g_assert(plxInternal != NULL);

    PlxFeatures *plxFeatures = g_hash_table_lookup(plxInternal->features, binc_device_get_address(device));
    if (plxFeatures != NULL) {
        g_hash_table_remove(plxInternal->features, binc_device_get_address(device));
    }
}

ServiceHandler *plx_service_handler_create() {
    PlxInternal *plxInternal = g_new0(PlxInternal, 1);
    plxInternal->features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify) plx_free_features);

    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = plxInternal;
    handler->uuid = PLX_SERVICE_UUID;
    handler->on_characteristics_discovered = &plx_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &plx_onNotificationStateUpdated;
    handler->on_characteristic_write = NULL;
    handler->on_characteristic_changed = &plx_onCharacteristicChanged;
    handler->on_device_disconnected = &plx_onDeviceDisconnected;
    handler->service_handler_free = &plx_free;
    return handler;
}