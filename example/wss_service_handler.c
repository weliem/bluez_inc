//
// Created by martijn on 29-12-21.
//

#include "wss_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"
#include "observation_units.h"
#include "observation.h"
#include "observation_location.h"
#include "../utility.h"

#define TAG "WSS_Service_Handler"

static void wss_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));

    Characteristic *datetime_char = binc_device_get_characteristic(device, WSS_SERVICE_UUID, WSS_DATE_TIME_CHAR_UUID);
    if (datetime_char != NULL && binc_characteristic_supports_write(datetime_char, WITH_RESPONSE)) {
        GByteArray *timeBytes = binc_get_date_time();
        binc_characteristic_write(datetime_char, timeBytes, WITH_RESPONSE);
        g_byte_array_free(timeBytes, TRUE);
    }

    Characteristic *measurement_char = binc_device_get_characteristic(device, WSS_SERVICE_UUID, WSS_MEASUREMENT_CHAR_UUID);
    if (measurement_char != NULL && binc_characteristic_supports_notify(measurement_char)) {
        binc_characteristic_start_notify(measurement_char);
    }
}

static void wss_onNotificationStateUpdated(ServiceHandler *service_handler,
                                           Device *device,
                                           Characteristic *characteristic,
                                           const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }
    log_debug(TAG, "characteristic <%s> notifying %s", uuid, is_notifying ? "true" : "false");
}

static void wss_onCharacteristicWrite(ServiceHandler *service_handler,
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

typedef struct wss_measurement {
    double value;
    ObservationUnit unit;
    GDateTime *timestamp;
    guint8 user_id;
    guint16 bmi;
    guint16 height;

} WeightMeasurement;

static Observation *wss_measurement_as_observation(WeightMeasurement *measurement) {
    Observation *observation = g_new0(Observation, 1);
    observation->value = (float) measurement->value;
    observation->unit = measurement->unit;
    observation->duration_msec = 1000;
    observation->type = BODY_WEIGHT;
    observation->timestamp = g_date_time_ref(measurement->timestamp);
    observation->received = g_date_time_new_now_local();
    observation->location = LOCATION_FOOT;
    return observation;
}

static WeightMeasurement *wss_create_measurement(const GByteArray *byteArray) {
    WeightMeasurement *measurement = g_new0(WeightMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timestamp_present = (flags & 0x02) > 0;
    gboolean user_id_present = (flags & 0x04) > 0;
    gboolean bmi_height_present = (flags & 0x08) > 0;

    measurement->unit = (flags & 0x01) > 0 ? LBS : KG;
    double multiplier = (measurement->unit == KG) ? 0.005 : 0.01;
    measurement->value = parser_get_uint16(parser) * multiplier;
    measurement->timestamp = timestamp_present ? parser_get_date_time(parser) : g_date_time_new_now_local();
    measurement->user_id = user_id_present ? parser_get_uint8(parser) : 255;
    measurement->bmi = bmi_height_present ? parser_get_uint16(parser) : 0xFFFF;
    measurement->height = bmi_height_present ? parser_get_uint16(parser) : 0xFFFF;

    parser_free(parser);
    return measurement;
}

void wss_measurement_free(WeightMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    g_free(measurement);
}

static void wss_onCharacteristicChanged(ServiceHandler *service_handler,
                                        Device *device,
                                        Characteristic *characteristic,
                                        const GByteArray *byteArray,
                                        const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    GList *observation_list = NULL;

    if (g_str_equal(uuid, WSS_MEASUREMENT_CHAR_UUID)) {
        WeightMeasurement *measurement = wss_create_measurement(byteArray);

        Observation *observation = wss_measurement_as_observation(measurement);
        wss_measurement_free(measurement);

        observation_list = g_list_append(observation_list, observation);
        if (service_handler->observations_callback != NULL) {
            DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
            service_handler->observations_callback(observation_list, deviceInfo);
        }
        observation_list_free(observation_list);
    }
}

static void wss_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing WSS private_data");
}

ServiceHandler *wss_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = WSS_SERVICE_UUID;
    handler->on_characteristics_discovered = &wss_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &wss_onNotificationStateUpdated;
    handler->on_characteristic_write = &wss_onCharacteristicWrite;
    handler->on_characteristic_changed = &wss_onCharacteristicChanged;
    handler->on_device_disconnected = NULL;
    handler->service_handler_free = &wss_free;
    return handler;
}