//
// Created by martijn on 07-11-21.
//

#include "cts_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"

#define TAG "CTS_Service_Handler"

typedef struct cts_internal {
    GHashTable *currenttime_is_set;
} CtsInternal;

static void write_current_time(Characteristic *current_time) {
    GByteArray *timeBytes = binc_get_current_time();
    log_debug(TAG, "writing current time");
    binc_characteristic_write(current_time, timeBytes, WITH_RESPONSE);
    g_byte_array_free(timeBytes, TRUE);
}

static gboolean isOmron(Device *device) {
    return g_str_has_prefix(binc_device_get_name(device), "BLESmart_") ||
           g_str_has_prefix(binc_device_get_name(device), "BLEsmart_");
}

static void cts_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));
    Characteristic *current_time = binc_device_get_characteristic(device, CTS_SERVICE, CURRENT_TIME_CHAR);
    if (current_time != NULL) {
        if (binc_characteristic_supports_notify(current_time)) {
            binc_characteristic_start_notify(current_time);
        } else if (binc_characteristic_supports_read(current_time)) {
            binc_characteristic_read(current_time);
        }

        if (binc_characteristic_supports_write(current_time, WITH_RESPONSE) && !isOmron(device)) {
            write_current_time(current_time);
        }
    }
}


static void
cts_onNotificationStateUpdated(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                               GError *error) {

}

static void cts_onCharacteristicWrite(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                      GByteArray *byteArray, GError *error) {

}

static void handle_current_time(GByteArray *byteArray) {
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    GDateTime *dateTime = parser_get_date_time(parser);
    parser_free(parser);

    if (dateTime != NULL) {
        char *time_string = g_date_time_format(dateTime, "%F %R:%S");
        if (time_string != NULL) {
            log_debug(TAG, "currenttime=%s", time_string);
            g_free(time_string);
        }
        g_date_time_unref(dateTime);
    }
}

static void cts_onCharacteristicChanged(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                        GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        if (g_str_equal(uuid, CURRENT_TIME_CHAR)) {
            handle_current_time(byteArray);

            if (isOmron(device)) {
                CtsInternal *ctsInternal = (CtsInternal *) service_handler->private_data;
                char *is_set = g_hash_table_lookup(ctsInternal->currenttime_is_set, binc_device_get_address(device));
                if (is_set == NULL) {
                    write_current_time(characteristic);
                    g_hash_table_insert(ctsInternal->currenttime_is_set, g_strdup(binc_device_get_address(device)),
                                        g_strdup("set"));
                }
            }
        }
    }
}

static void cts_onDeviceDisconnected(ServiceHandler *service_handler, Device *device) {
    log_debug(TAG, "onDeviceDisconnected");
    CtsInternal *ctsInternal = (CtsInternal *) service_handler->private_data;
    char *is_set = g_hash_table_lookup(ctsInternal->currenttime_is_set, binc_device_get_address(device));
    if (is_set != NULL) {
        g_hash_table_remove(ctsInternal->currenttime_is_set, binc_device_get_address(device));
    }
}

static void cts_free(ServiceHandler *service_handler) {
    CtsInternal *ctsInternal = (CtsInternal *) service_handler->private_data;
    g_hash_table_destroy(ctsInternal->currenttime_is_set);
    g_free(ctsInternal);
    log_debug(TAG, "freeing CTS private_data");
}

ServiceHandler *cts_service_handler_create() {
    CtsInternal *ctsInternal = g_new0(CtsInternal, 1);
    ctsInternal->currenttime_is_set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = ctsInternal;
    handler->uuid = CTS_SERVICE;
    handler->on_characteristics_discovered = &cts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &cts_onNotificationStateUpdated;
    handler->on_characteristic_write = &cts_onCharacteristicWrite;
    handler->on_characteristic_changed = &cts_onCharacteristicChanged;
    handler->on_device_disconnected = &cts_onDeviceDisconnected;
    handler->service_handler_free = &cts_free;
    return handler;
}