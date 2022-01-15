//
// Created by martijn on 12-01-22.
//

#include "plx_service_handler.h"
#include "../../characteristic.h"
#include "../../parser.h"
#include "../../logger.h"
#include "../../device.h"
#include "../observation_units.h"
#include "../observation.h"

#define TAG "PLX_Service_Handler"

static void plx_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device, GList *characteristics) {
    log_debug(TAG, "discovered %d characteristics", g_list_length(characteristics));
    Characteristic *spot_measurement_char = binc_device_get_characteristic(device, PLX_SERVICE_UUID,
                                                                           SPOT_MEASUREMENT_CHAR_UUID);
    if (spot_measurement_char != NULL && binc_characteristic_supports_notify(spot_measurement_char)) {
        binc_characteristic_start_notify(spot_measurement_char);
    }
//    Characteristic *continuous_measurement_char = binc_device_get_characteristic(device, PLX_SERVICE_UUID,
//                                                                                 CONTINUOUS_MEASUREMENT_CHAR_UUID);
//    if (continuous_measurement_char != NULL && binc_characteristic_supports_notify(continuous_measurement_char)) {
//        binc_characteristic_start_notify(continuous_measurement_char);
//    }
}

static void plx_onNotificationStateUpdated(ServiceHandler *service_handler,
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

typedef struct plx_measurement_status {
    gboolean isMeasurementOngoing;
    gboolean isEarlyEstimateData;
    gboolean isValidatedData;
    gboolean isFullyQualifiedData;
    gboolean isDataMeasurementStorage;
    gboolean isDataDemonstration;
    gboolean isDataTesting;
    gboolean isCalibrationOngoing;
    gboolean isMeasurementUnavailable;
    gboolean isQuestionableMeasurementDetected;
    gboolean isInvalidMeasurementDetected;
} MeasurementStatus;

typedef struct plx_spot_measurement {
    float spo2_value;
    float pulse_value;
    float pulse_amplitude_index;
    MeasurementStatus* measurement_status;
    guint16 sensor_status;
    GDateTime *timestamp;
    gboolean is_device_clock_set;
} SpotMeasurement;

static Observation *plx_spot_measurement_as_observation(SpotMeasurement *measurement) {
    Observation *observation = g_new0(Observation, 1);
    observation->value = measurement->spo2_value;
    observation->unit = PERCENT;
    observation->duration_msec = 5000;
    observation->type = BLOOD_OXYGEN_SATURATION;
    observation->timestamp = g_date_time_ref(measurement->timestamp);
    observation->received = g_date_time_new_now_local();
    observation->location = LOCATION_FINGER;
    return observation;
}

static MeasurementStatus* plx_create_measurement_status(guint16 status) {
    MeasurementStatus *measurementStatus = g_new0(MeasurementStatus, 1);
    measurementStatus->isMeasurementOngoing = (status & 0x0020) > 0;
    measurementStatus->isEarlyEstimateData = (status & 0x0040) > 0;
    measurementStatus->isValidatedData = (status & 0x0080) > 0;
    measurementStatus->isFullyQualifiedData = (status & 0x0100) > 0;
    measurementStatus->isDataMeasurementStorage = (status & 0x0200) > 0;
    measurementStatus->isDataDemonstration = (status & 0x0400) > 0;
    measurementStatus->isDataTesting = (status & 0x0800) > 0;
    measurementStatus->isCalibrationOngoing = (status & 0x1000) > 0;
    measurementStatus->isMeasurementUnavailable = (status & 0x2000) > 0;
    measurementStatus->isQuestionableMeasurementDetected = (status & 0x4000) > 0;
    measurementStatus->isInvalidMeasurementDetected = (status & 0x8000) > 0;
    return measurementStatus;
}

static SpotMeasurement *plx_create_spot_measurement(const GByteArray *byteArray) {
    SpotMeasurement *measurement = g_new0(SpotMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timestampPresent = (flags & 0x01) > 0;
    gboolean measurementStatusPresent = (flags & 0x02) > 0;
    gboolean sensorStatusPresent = (flags & 0x04) > 0;
    gboolean pulseAmplitudeIndexPresent = (flags & 0x08) > 0;
    measurement->is_device_clock_set = (flags & 0x10) == 0;

    measurement->spo2_value = parser_get_sfloat(parser);
    measurement->pulse_value = parser_get_sfloat(parser);
    measurement->timestamp = timestampPresent ? parser_get_date_time(parser) : NULL;
    measurement->measurement_status = plx_create_measurement_status(measurementStatusPresent ? parser_get_uint16(parser) : 0x0000);
    measurement->sensor_status = sensorStatusPresent ? parser_get_uint16(parser) : 0xFFFF;
    guint8 reserved_byte = sensorStatusPresent ? parser_get_uint8(parser) : 0xFF;
    measurement->pulse_amplitude_index = pulseAmplitudeIndexPresent ? parser_get_sfloat(parser) : 0x00;

    parser_free(parser);
    return measurement;
}

void plx_spot_measurement_free(SpotMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    if (measurement->measurement_status != NULL) {
        g_free(measurement->measurement_status);
    }
    g_free(measurement);
}

static void plx_onCharacteristicChanged(ServiceHandler *service_handler,
                                        Device *device,
                                        Characteristic *characteristic,
                                        const GByteArray *byteArray,
                                        const GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    if (g_str_equal(uuid, SPOT_MEASUREMENT_CHAR_UUID)) {
        SpotMeasurement *measurement = plx_create_spot_measurement(byteArray);

        if (measurement->timestamp == NULL) {
            log_debug(TAG, "Ignoring spot measurement without timestamp");
            return;
        }

        if (!measurement->measurement_status->isFullyQualifiedData) {
            log_debug(TAG, "Ignoring spot measurement because it is not a valid measurement");
            return;
        }

        Observation *observation = plx_spot_measurement_as_observation(measurement);
        plx_spot_measurement_free(measurement);

        GList *observation_list = NULL;
        observation_list = g_list_append(observation_list, observation);
        if (service_handler->observations_callback != NULL) {
            DeviceInfo *deviceInfo = get_device_info(binc_device_get_address(device));
            service_handler->observations_callback(observation_list, deviceInfo);
        }
        observation_list_free(observation_list);
    }
}

static void plx_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing HTS private_data");
}

ServiceHandler *plx_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = PLX_SERVICE_UUID;
    handler->on_characteristics_discovered = &plx_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &plx_onNotificationStateUpdated;
    handler->on_characteristic_write = NULL;
    handler->on_characteristic_changed = &plx_onCharacteristicChanged;
    handler->on_device_disconnected = NULL;
    handler->service_handler_free = &plx_free;
    return handler;
}