//
// Created by martijn on 04-11-21.
//

#include "hts_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"

#define TAG "HTS_Service_Handler"

#define TEMPERATURE_CHAR_UUID "00002a1c-0000-1000-8000-00805f9b34fb"

static void hts_onCharacteristicsDiscovered(gpointer *handler, Device *device) {

}

static void hts_onNotificationStateUpdated(gpointer *handler, Device *device, Characteristic *characteristic) {

}

static void hts_onCharacteristicWrite(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {

}

static void hts_onCharacteristicChanged(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    parser_set_offset(parser, 1);

    if (g_str_equal(uuid, TEMPERATURE_CHAR_UUID)) {
        float value = parser_get_float(parser);
        log_debug(TAG, "temperature %.1f", value);
    }
    parser_free(parser);
}


ServiceHandler* hts_service_handler_get() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->handler = NULL;
    handler->uuid = "00001809-0000-1000-8000-00805f9b34fb";
    handler->on_characteristics_discovered = &hts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &hts_onNotificationStateUpdated;
    handler->on_characteristic_write = &hts_onCharacteristicWrite;
    handler->on_characteristic_changed = &hts_onCharacteristicChanged;
    return handler;
}