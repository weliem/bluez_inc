//
// Created by martijn on 22-01-22.
//

#include "glx_service_handler.h"
#include "../../characteristic.h"
#include "../../parser.h"
#include "../../logger.h"
#include "../../device.h"
#include "../observation.h"
#include "glx_feature.h"
#include "glx_measurement.h"

#define TAG "GLX_Service_Handler"

#define OP_CODE_REPORT_STORED_RECORDS 1
#define OPERATOR_ALL_RECORDS 1

typedef struct glx_internal {
    GHashTable *features;
} GlxInternal;

static void glx_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));

    binc_device_read_char(device, GLX_SERVICE_UUID, GLX_FEATURE_CHAR_UUID);
    binc_device_start_notify(device, GLX_SERVICE_UUID, GLX_MEASUREMENT_CHAR_UUID);
    binc_device_start_notify(device, GLX_SERVICE_UUID, GLX_MEASUREMENT_CONTEXT_CHAR_UUID);
    binc_device_start_notify(device, GLX_SERVICE_UUID, GLX_RACP_CHAR_UUID);
}

static void get_measurements(Device *device) {
    guint8 command[] = {OP_CODE_REPORT_STORED_RECORDS, OPERATOR_ALL_RECORDS};
    GByteArray *byteArray = g_byte_array_new_take(command, 2);
    binc_device_write_char(device, GLX_SERVICE_UUID, GLX_RACP_CHAR_UUID, byteArray, WITH_RESPONSE);
    g_byte_array_free(byteArray, FALSE);
}

static void glx_onNotificationStateUpdated(ServiceHandler *service_handler,
                                           Device *device,
                                           Characteristic *characteristic,
                                           const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (g_str_equal(uuid, GLX_RACP_CHAR_UUID) && binc_characteristic_is_notifying(characteristic)) {
        get_measurements(device);
    }
}

static void handleFeature(const ServiceHandler *service_handler, const Device *device,const GByteArray *byteArray ) {
    GlxFeatures *glxFeatures = glx_create_features(byteArray);
    g_assert(glxFeatures != NULL);

    GlxInternal *glxInternal = (GlxInternal *) service_handler->private_data;
    g_assert(glxInternal != NULL);

    g_hash_table_insert(glxInternal->features, g_strdup(binc_device_get_address(device)), glxFeatures);

    log_debug(TAG, "got GLX feature");
}

static void handleMeasurement(const ServiceHandler *service_handler, const Device *device,const GByteArray *byteArray ) {
    GlxMeasurement *measurement = glx_measurement_create(byteArray);
    log_debug(TAG, "got GLX measurement");
}

static void glx_onCharacteristicChanged(ServiceHandler *service_handler,
                                        Device *device,
                                        Characteristic *characteristic,
                                        const GByteArray *byteArray,
                                        const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray == NULL) return;

    g_str_equal(uuid, GLX_FEATURE_CHAR_UUID) ? handleFeature(service_handler, device, byteArray) :
    g_str_equal(uuid, GLX_MEASUREMENT_CHAR_UUID) ? handleMeasurement(service_handler, device, byteArray) : NULL;
}

static void glx_free(ServiceHandler *service_handler) {
    GlxInternal *plxInternal = (GlxInternal *) service_handler->private_data;
    g_assert(plxInternal != NULL);

    g_hash_table_destroy(plxInternal->features);
    g_free(plxInternal);
    log_debug(TAG, "freeing GLX private_data");
}

static void glx_onDeviceDisconnected(ServiceHandler *service_handler, Device *device) {
    GlxInternal *plxInternal = (GlxInternal *) service_handler->private_data;
    g_assert(plxInternal != NULL);

    GlxFeatures *glxFeatures = g_hash_table_lookup(plxInternal->features, binc_device_get_address(device));
    if (glxFeatures != NULL) {
        g_hash_table_remove(plxInternal->features, binc_device_get_address(device));
    }
}

ServiceHandler *glx_service_handler_create() {
    GlxInternal *glxInternal = g_new0(GlxInternal, 1);
    glxInternal->features = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = glxInternal;
    handler->uuid = GLX_SERVICE_UUID;
    handler->on_characteristics_discovered = &glx_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &glx_onNotificationStateUpdated;
    handler->on_characteristic_write = NULL;
    handler->on_characteristic_changed = &glx_onCharacteristicChanged;
    handler->on_device_disconnected = &glx_onDeviceDisconnected;
    handler->service_handler_free = &glx_free;
    return handler;
}