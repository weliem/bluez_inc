//
// Created by martijn on 07-11-21.
//

#include "dis_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"
#include "device_info.h"

#define TAG "DIS_Service_Handler"

static void dis_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));
    Characteristic *manufacturer = binc_device_get_characteristic(device, DIS_SERVICE, DIS_MANUFACTURER_CHAR);
    if (manufacturer != NULL && binc_characteristic_supports_read(manufacturer)) {
        binc_characteristic_read(manufacturer);
    }
    Characteristic *model = binc_device_get_characteristic(device, DIS_SERVICE, DIS_MODEL_CHAR);
    if (manufacturer != NULL && binc_characteristic_supports_read(model)) {
        binc_characteristic_read(model);
    }
    Characteristic *serial = binc_device_get_characteristic(device, DIS_SERVICE, DIS_SERIAL_NUMBER_CHAR);
    if (serial != NULL && binc_characteristic_supports_read(serial)) {
        binc_characteristic_read(serial);
    }
}

static void
dis_onNotificationStateUpdated(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                               GError *error) {

}

static void dis_onCharacteristicWrite(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                      GByteArray *byteArray, GError *error) {

}

static void dis_onCharacteristicChanged(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                        GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
        GString *parsed_string = parser_get_string(parser);
        if (g_str_equal(uuid, DIS_MANUFACTURER_CHAR)) {
            log_debug(TAG, "manufacturer = %s", parsed_string->str);
            device_info_set_manufacturer(deviceInfo, parsed_string->str);
        } else if (g_str_equal(uuid, DIS_MODEL_CHAR)) {
            log_debug(TAG, "model = %s", parsed_string->str);
            device_info_set_model(deviceInfo, parsed_string->str);
        } else if (g_str_equal(uuid, DIS_SERIAL_NUMBER_CHAR)) {
            log_debug(TAG, "serial = %s", parsed_string->str);
            device_info_set_serialnumber(deviceInfo, parsed_string->str);
        }
        g_string_free(parsed_string, TRUE);
        parser_free(parser);
    }
}

static void dis_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing DIS private_data");
}

ServiceHandler *dis_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = DIS_SERVICE;
    handler->on_characteristics_discovered = &dis_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &dis_onNotificationStateUpdated;
    handler->on_characteristic_write = &dis_onCharacteristicWrite;
    handler->on_characteristic_changed = &dis_onCharacteristicChanged;
    handler->on_device_disconnected = NULL;
    handler->service_handler_free = &dis_free;
    return handler;
}