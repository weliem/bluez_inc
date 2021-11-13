//
// Created by martijn on 07-11-21.
//

#include "cts_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"

#define TAG "CTS_Service_Handler"

static void cts_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device) {
    Characteristic *current_time = binc_device_get_characteristic(device, CTS_SERVICE, CURRENT_TIME_CHAR);
    if (current_time != NULL) {
        if (binc_characteristic_supports_notify(current_time)) {
            binc_characteristic_start_notify(current_time);
        } else if (binc_characteristic_supports_read(current_time)) {
            binc_characteristic_read(current_time);
        }

        if (binc_characteristic_supports_write(current_time, WITH_RESPONSE)) {
            GByteArray *timeBytes = binc_get_current_time();
            log_debug(TAG, "writing current time");
            binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
            g_byte_array_free(timeBytes, TRUE);
        }
    }
}

static void cts_onNotificationStateUpdated(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GError *error) {

}

static void cts_onCharacteristicWrite(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {

}

static void cts_onCharacteristicChanged(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
        if (g_str_equal(uuid, CURRENT_TIME_CHAR)) {
            GDateTime *dateTime = parser_get_date_time(parser);
            if (dateTime != NULL) {
                char *time_string = g_date_time_format(dateTime, "%F %R:%S");
                log_debug(TAG, "currenttime=%s", time_string);
                g_free(time_string);
                g_date_time_unref(dateTime);
            }
        }
        parser_free(parser);
    }
}

static void cts_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing CTS private_data");
}

ServiceHandler* cts_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = CTS_SERVICE;
    handler->on_characteristics_discovered = &cts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &cts_onNotificationStateUpdated;
    handler->on_characteristic_write = &cts_onCharacteristicWrite;
    handler->on_characteristic_changed = &cts_onCharacteristicChanged;
    handler->service_handler_free = &cts_free;
    return handler;
}