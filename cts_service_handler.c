//
// Created by martijn on 07-11-21.
//

#include "cts_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"
#include "device.h"

#define TAG "CTS_Service_Handler"

static void cts_onCharacteristicsDiscovered(gpointer *handler, Device *device) {
    Characteristic *current_time = binc_device_get_characteristic(device, CTS_SERVICE, CURRENT_TIME_CHAR);
    if (current_time != NULL) {
        if (binc_characteristic_supports_write(current_time, WITH_RESPONSE)) {
            GByteArray *timeBytes = binc_get_current_time();
            log_debug(TAG, "writing current time");
            binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
            g_byte_array_free(timeBytes, TRUE);
        }
        if (binc_characteristic_supports_notify(current_time)) {
            binc_characteristic_start_notify(current_time);
        }
    }
}

static void cts_onNotificationStateUpdated(gpointer *handler, Device *device, Characteristic *characteristic, GError *error) {

}

static void cts_onCharacteristicWrite(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {

}

static void cts_onCharacteristicChanged(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
//        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
//        if (g_str_equal(uuid, MANUFACTURER_CHAR)) {
//            GString *manufacturer = parser_get_string(parser);
//            log_debug(TAG, "manufacturer = %s", manufacturer->str);
//            g_string_free(manufacturer, TRUE);
//        } else if (g_str_equal(uuid, MODEL_CHAR)) {
//            GString *model = parser_get_string(parser);
//            log_debug(TAG, "model = %s", model->str);
//            g_string_free(model, TRUE);
//        }
//        parser_free(parser);
    }
}

static void cts_free(gpointer *handler) {
    log_debug(TAG, "freeing CTS handler");
}

ServiceHandler* cts_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->handler = NULL;
    handler->uuid = CTS_SERVICE;
    handler->on_characteristics_discovered = &cts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &cts_onNotificationStateUpdated;
    handler->on_characteristic_write = &cts_onCharacteristicWrite;
    handler->on_characteristic_changed = &cts_onCharacteristicChanged;
    handler->service_handler_free = &cts_free;
    return handler;
}