//
// Created by martijn on 07-11-21.
//

#include "blp_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"
#include "device.h"
#include "observation_units.h"

#define TAG "BLP_Service_Handler"

typedef struct blp_measurement {
    float systolic;
    float diastolic;
    float mean_arterial_pressure;
    ObservationUnit unit;
    GDateTime *timestamp;
    float pulse_rate;
    guint8 user_id;
    guint16 status;
} BloodPressureMeasurement;

static BloodPressureMeasurement *blp_create_measurement(GByteArray *byteArray) {
    BloodPressureMeasurement *measurement = g_new0(BloodPressureMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timestamp_present = (flags & 0x02) > 0;
    gboolean pulse_present = (flags & 0x04) > 0;
    gboolean user_id_present = (flags & 0x08) > 0;
    gboolean status_present = (flags & 0x10) > 0;

    measurement->systolic = parser_get_sfloat(parser);
    measurement->diastolic = parser_get_sfloat(parser);
    measurement->mean_arterial_pressure = parser_get_sfloat(parser);
    measurement->unit = (flags & 0x01) > 0 ? KPA : MMHG;
    measurement->timestamp = timestamp_present ? parser_get_date_time(parser) : NULL;
    measurement->pulse_rate = pulse_present ? parser_get_sfloat(parser) : 0;
    measurement->user_id = user_id_present ? parser_get_uint8(parser) : 0xFF;
    measurement->status = status_present ? parser_get_uint16(parser) : 0xFFFF;

    parser_free(parser);
    return measurement;
}

void blp_measurement_free(BloodPressureMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    g_free(measurement);
}

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
        if (g_str_equal(uuid, BLOODPRESSURE_CHAR)) {
            BloodPressureMeasurement *measurement = blp_create_measurement(byteArray);

            // Log the measurement
            char* time_string = g_date_time_format(measurement->timestamp, "%F %R:%S");
            log_debug(TAG, "systolic=%.0f, diastolic=%.0f, unit=%s, pulse=%.0f, timestamp=%s",
                      measurement->systolic,
                      measurement->diastolic,
                      measurement->unit == MMHG ? "mmHg":"kPa",
                      measurement->pulse_rate,
                      time_string
            );
            g_free(time_string);

            blp_measurement_free(measurement);
        }
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