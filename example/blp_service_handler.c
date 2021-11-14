//
// Created by martijn on 07-11-21.
//

#include "blp_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"
#include "observation_units.h"
#include "observation.h"

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

static GList *blp_measurement_as_observations(BloodPressureMeasurement *measurement) {
    GList *observation_list = NULL;

    Observation *observation1 = g_new0(Observation, 1);
    observation1->value = measurement->systolic;
    observation1->unit = measurement->unit;
    observation1->duration_msec = 30000;
    observation1->type = BLOOD_PRESSURE_SYSTOLIC;
    observation1->timestamp = g_date_time_ref(measurement->timestamp);
    observation1->received = g_date_time_new_now_local();
    observation1->location = LOCATION_ARM;
    observation_list = g_list_append(observation_list, observation1);

    Observation *observation2 = g_new0(Observation, 1);
    observation2->value = measurement->diastolic;
    observation2->unit = measurement->unit;
    observation2->duration_msec = 30000;
    observation2->type = BLOOD_PRESSURE_DIASTOLIC;
    observation2->timestamp = g_date_time_ref(measurement->timestamp);
    observation2->received = g_date_time_new_now_local();
    observation2->location = LOCATION_ARM;
    observation_list = g_list_append(observation_list, observation2);

    Observation *observation3 = g_new0(Observation, 1);
    observation3->value = measurement->mean_arterial_pressure;
    observation3->unit = measurement->unit;
    observation3->duration_msec = 30000;
    observation3->type = BLOOD_PRESSURE_MEAN;
    observation3->timestamp = g_date_time_ref(measurement->timestamp);
    observation3->received = g_date_time_new_now_local();
    observation3->location = LOCATION_ARM;
    observation_list = g_list_append(observation_list, observation3);

    Observation *observation4 = g_new0(Observation, 1);
    observation4->value = measurement->pulse_rate;
    observation4->unit = BPM;
    observation4->duration_msec = 30000;
    observation4->type = BLOOD_PRESSURE_PULSE;
    observation4->timestamp = g_date_time_ref(measurement->timestamp);
    observation4->received = g_date_time_new_now_local();
    observation4->location = LOCATION_ARM;
    observation_list = g_list_append(observation_list, observation4);

    return observation_list;
}

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

static void blp_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device) {
    Characteristic *blp_measurement = binc_device_get_characteristic(device, BLP_SERVICE_UUID, BLOODPRESSURE_CHAR_UUID);
    if (blp_measurement != NULL && binc_characteristic_supports_notify(blp_measurement)) {
        binc_characteristic_start_notify(blp_measurement);
    }
}

static void
blp_onNotificationStateUpdated(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }
    log_debug(TAG, "characteristic <%s> notifying %s", uuid, is_notifying ? "true" : "false");
}

static void
blp_onCharacteristicWrite(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GByteArray *byteArray,
                          GError *error) {

}

static void blp_onCharacteristicChanged(ServiceHandler *service_handler, Device *device,
                            Characteristic *characteristic, GByteArray *byteArray, GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);

    if (error != NULL) {
        log_debug(TAG, "failed to read '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }

    if (byteArray != NULL) {
        if (g_str_equal(uuid, BLOODPRESSURE_CHAR_UUID)) {
            BloodPressureMeasurement *measurement = blp_create_measurement(byteArray);
            GList *observations_list = blp_measurement_as_observations(measurement);

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

            if (service_handler->observations_callback != NULL) {
                service_handler->observations_callback(observations_list);
            }

            observation_list_free(observations_list);
        }
    }
}

static void blp_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing BLP private_data");
}

ServiceHandler *blp_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = BLP_SERVICE_UUID;
    handler->on_characteristics_discovered = &blp_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &blp_onNotificationStateUpdated;
    handler->on_characteristic_write = &blp_onCharacteristicWrite;
    handler->on_characteristic_changed = &blp_onCharacteristicChanged;
    handler->service_handler_free = &blp_free;
    return handler;
}