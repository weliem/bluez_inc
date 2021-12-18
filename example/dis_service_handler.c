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

#define DIS_MANUFACTURER_CHAR "00002a29-0000-1000-8000-00805f9b34fb"
#define DIS_MODEL_CHAR "00002a24-0000-1000-8000-00805f9b34fb"
#define DIS_SERIAL_NUMBER_CHAR "00002a25-0000-1000-8000-00805f9b34fb"
#define DIS_FIRMWARE_REVISION_CHAR "00002a26-0000-1000-8000-00805f9b34fb"
#define DIS_HARDWARE_REVISION_CHAR "00002a27-0000-1000-8000-00805f9b34fb"
#define DIS_SOFTWARE_REVISION_CHAR "00002a28-0000-1000-8000-00805f9b34fb"

// Array to define which characteristics to read and in which order
static const char *dis_characteristics[] = {
        DIS_MANUFACTURER_CHAR,
        DIS_MODEL_CHAR,
        DIS_SERIAL_NUMBER_CHAR,
        DIS_FIRMWARE_REVISION_CHAR,
        DIS_HARDWARE_REVISION_CHAR,
        DIS_SOFTWARE_REVISION_CHAR
};

static void dis_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));

    // Read all 'supported' characteristics
    int characteristics_len = sizeof(dis_characteristics) / sizeof(char *);
    for (int i = 0; i < characteristics_len; i++) {
        Characteristic *characteristic = binc_device_get_characteristic(device, DIS_SERVICE, dis_characteristics[i]);
        if (characteristic != NULL && binc_characteristic_supports_read(characteristic)) {
            binc_characteristic_read(characteristic);
        }
    }
}

static void dis_onCharacteristicChanged(ServiceHandler *service_handler,
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
    } else if (g_str_equal(uuid, DIS_FIRMWARE_REVISION_CHAR)) {
        log_debug(TAG, "firmware = %s", parsed_string->str);
        device_info_set_firmware_version(deviceInfo, parsed_string->str);
    } else if (g_str_equal(uuid, DIS_HARDWARE_REVISION_CHAR)) {
        log_debug(TAG, "hardware = %s", parsed_string->str);
        device_info_set_hardware_version(deviceInfo, parsed_string->str);
    } else if (g_str_equal(uuid, DIS_SOFTWARE_REVISION_CHAR)) {
        log_debug(TAG, "software = %s", parsed_string->str);
        device_info_set_software_version(deviceInfo, parsed_string->str);
    }

    if (parsed_string != NULL) {
        g_string_free(parsed_string, TRUE);
    }
    parser_free(parser);
}

ServiceHandler *dis_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = DIS_SERVICE;
    handler->on_characteristics_discovered = &dis_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = NULL;
    handler->on_characteristic_write = NULL;
    handler->on_characteristic_changed = &dis_onCharacteristicChanged;
    handler->on_device_disconnected = NULL;
    handler->service_handler_free = NULL;
    return handler;
}