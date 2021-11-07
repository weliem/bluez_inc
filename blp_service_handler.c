//
// Created by martijn on 07-11-21.
//

#include "blp_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"
#include "device.h"

#define TAG "BLP_Service_Handler"

static void blp_onCharacteristicsDiscovered(gpointer *handler, Device *device) {
    Characteristic *blp_measurement = binc_device_get_characteristic(device, BLP_SERVICE, BLOODPRESSURE_CHAR);
    if (blp_measurement != NULL && binc_characteristic_supports_notify(blp_measurement)) {
        binc_characteristic_start_notify(blp_measurement);
    }
}

static void
blp_onNotificationStateUpdated(gpointer *handler, Device *device, Characteristic *characteristic, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }
    log_debug(TAG, "characteristic <%s> notifying %s", uuid, is_notifying ? "true" : "false");
}

static void
blp_onCharacteristicWrite(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray,
                          GError *error) {

}

static void
blp_onCharacteristicChanged(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray,
                            GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
        parser_set_offset(parser, 1);

        if (g_str_equal(uuid, BLOODPRESSURE_CHAR)) {
            float systolic = parser_get_sfloat(parser);
            float diastolic = parser_get_sfloat(parser);
            log_debug(TAG, "bpm %.0f/%.0f", systolic, diastolic);
        }
        parser_free(parser);
    }
}

static void blp_free(gpointer *handler) {
    log_debug(TAG, "freeing BLP handler");
}

ServiceHandler *blp_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->handler = NULL;
    handler->uuid = BLP_SERVICE;
    handler->on_characteristics_discovered = &blp_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &blp_onNotificationStateUpdated;
    handler->on_characteristic_write = &blp_onCharacteristicWrite;
    handler->on_characteristic_changed = &blp_onCharacteristicChanged;
    handler->service_handler_free = &blp_free;
    return handler;
}