//
// Created by martijn on 07-11-21.
//

#include "dis_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"
#include "device.h"

#define TAG "DIS_Service_Handler"

static void dis_onCharacteristicsDiscovered(gpointer *handler, Device *device) {
    Characteristic *manufacturer = binc_device_get_characteristic(device, DIS_SERVICE, MANUFACTURER_CHAR);
    if (manufacturer != NULL && binc_characteristic_supports_read(manufacturer)) {
        binc_characteristic_read(manufacturer);
    }
    Characteristic *model = binc_device_get_characteristic(device, DIS_SERVICE, MODEL_CHAR);
    if (manufacturer != NULL && binc_characteristic_supports_read(model)) {
        binc_characteristic_read(model);
    }
}

static void dis_onNotificationStateUpdated(gpointer *handler, Device *device, Characteristic *characteristic, GError *error) {

}

static void dis_onCharacteristicWrite(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {

}

static void dis_onCharacteristicChanged(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
        if (g_str_equal(uuid, MANUFACTURER_CHAR)) {
            GString *manufacturer = parser_get_string(parser);
            log_debug(TAG, "manufacturer = %s", manufacturer->str);
            g_string_free(manufacturer, TRUE);
        } else if (g_str_equal(uuid, MODEL_CHAR)) {
            GString *model = parser_get_string(parser);
            log_debug(TAG, "model = %s", model->str);
            g_string_free(model, TRUE);
        }
        parser_free(parser);
    }
}

static void dis_free(gpointer *handler) {
    log_debug(TAG, "freeing DIS handler");
}

ServiceHandler* dis_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->handler = NULL;
    handler->uuid = DIS_SERVICE;
    handler->on_characteristics_discovered = &dis_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &dis_onNotificationStateUpdated;
    handler->on_characteristic_write = &dis_onCharacteristicWrite;
    handler->on_characteristic_changed = &dis_onCharacteristicChanged;
    handler->service_handler_free = &dis_free;
    return handler;
}