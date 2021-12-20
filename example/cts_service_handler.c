//
// Created by martijn on 07-11-21.
//

#include "cts_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"
#include "../utility.h"

#define TAG "CTS_Service_Handler"

#define CURRENT_TIME_CHAR "00002a2b-0000-1000-8000-00805f9b34fb"
#define LOCAL_TIME_INFORMATION_CHAR "00002a0f-0000-1000-8000-00805f9b34fb"
#define REFERENCE_TIME_INFORMATION_CHAR "00002a14-0000-1000-8000-00805f9b34fb"

#define CURRENT_TIME_LENGTH 10
#define LOCAL_TIME_LENGTH 2

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

    Characteristic *local_time_info = binc_device_get_characteristic(device, CTS_SERVICE, LOCAL_TIME_INFORMATION_CHAR);
    if (local_time_info != NULL && binc_characteristic_supports_read(local_time_info)) {
        binc_characteristic_read(local_time_info);
    }

    Characteristic *reference_time_info = binc_device_get_characteristic(device, CTS_SERVICE, REFERENCE_TIME_INFORMATION_CHAR);
    if (reference_time_info != NULL && binc_characteristic_supports_read(reference_time_info)) {
        binc_characteristic_read(reference_time_info);
    }
}

static void cts_onNotificationStateUpdated(ServiceHandler *service_handler,
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

static void cts_onCharacteristicWrite(ServiceHandler *service_handler,
                                      Device *device,
                                      Characteristic *characteristic,
                                      const GByteArray *byteArray,
                                      const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to write to '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    GString *byteArrayStr = g_byte_array_as_hex(byteArray);
    log_debug(TAG, "success writing <%s> to <%s>", byteArrayStr->str, binc_characteristic_get_uuid(characteristic));
    g_string_free(byteArrayStr, TRUE);
}

static void handle_current_time(Device *device, const GByteArray *byteArray) {
    if (byteArray->len != CURRENT_TIME_LENGTH) {
        log_debug(TAG, "invalid current time received");
        return;
    }

    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    GDateTime *dateTime = parser_get_date_time(parser);
    parser_free(parser);

    if (dateTime != NULL) {
        DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
        device_info_set_device_time(deviceInfo, dateTime);

        char *time_string = g_date_time_format(dateTime, "%F %R:%S");
        if (time_string != NULL) {
            log_debug(TAG, "currenttime=%s", time_string);
            g_free(time_string);
        }
        g_date_time_unref(dateTime);
    }
}

static void handle_local_time_info(Device *device, const GByteArray *byteArray) {
    if (byteArray->len != LOCAL_TIME_LENGTH) {
        log_debug(TAG, "invalid local time received");
        return;
    }

    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    gint8 cts_timezone = parser_get_sint8(parser);
    guint8 cts_raw_dst_offset = parser_get_uint8(parser);
    parser_free(parser);

    log_debug(TAG, "local time info { timezone=%d, dst_offset=%d", cts_timezone, cts_raw_dst_offset);
}

static void cts_onCharacteristicChanged(ServiceHandler *service_handler,
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

    if (g_str_equal(uuid, CURRENT_TIME_CHAR)) {
        handle_current_time(device, byteArray);

        if (isOmron(device)) {
            CtsInternal *ctsInternal = (CtsInternal *) service_handler->private_data;
            const char *is_set = g_hash_table_lookup(ctsInternal->currenttime_is_set, binc_device_get_address(device));
            if (is_set == NULL) {
                write_current_time(characteristic);
                g_hash_table_insert(ctsInternal->currenttime_is_set, g_strdup(binc_device_get_address(device)),
                                    g_strdup("set"));
            }
        }
    } else if (g_str_equal(uuid, LOCAL_TIME_INFORMATION_CHAR)) {
        handle_local_time_info(device, byteArray);
    } else if (g_str_equal(uuid, REFERENCE_TIME_INFORMATION_CHAR)) {
        log_debug(TAG, "got reference time info");
    }
}

static void cts_onDeviceDisconnected(ServiceHandler *service_handler, Device *device) {
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