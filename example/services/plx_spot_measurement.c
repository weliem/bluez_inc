//
// Created by martijn on 16-01-22.
//

#include "plx_spot_measurement.h"

#include "../../parser.h"
#include "../observation.h"

GList *plx_spot_measurement_as_observations(SpotMeasurement *measurement) {
    GList *observation_list = NULL;

    Observation *spo2_observation = g_new0(Observation, 1);
    spo2_observation->value = measurement->spo2_value;
    spo2_observation->unit = PERCENT;
    spo2_observation->duration_msec = 5000;
    spo2_observation->type = BLOOD_OXYGEN_SATURATION;
    spo2_observation->timestamp = g_date_time_ref(measurement->timestamp);
    spo2_observation->received = g_date_time_new_now_local();
    spo2_observation->location = LOCATION_FINGER;
    observation_list = g_list_append(observation_list, spo2_observation);

    Observation *pulse_observation = g_new0(Observation, 1);
    pulse_observation->value = measurement->pulse_value;
    pulse_observation->unit = BPM;
    pulse_observation->duration_msec = 5000;
    pulse_observation->type = PULSE_OXIMETRY_PULSE;
    pulse_observation->timestamp = g_date_time_ref(measurement->timestamp);
    pulse_observation->received = g_date_time_new_now_local();
    pulse_observation->location = LOCATION_FINGER;
    observation_list = g_list_append(observation_list, pulse_observation);

    return observation_list;
}

static MeasurementStatus *plx_create_measurement_status(guint16 status) {
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

static SensorStatus *plx_create_sensor_status(guint16 status) {
    SensorStatus *sensorStatus = g_new0(SensorStatus, 1);
    sensorStatus->isExtendedDisplayUpdateOngoing = (status & 0x0001) > 0;
    sensorStatus->isEquipmentMalfunctionDetected = (status & 0x0002) > 0;
    sensorStatus->isSignalProcessingIrregularityDetected = (status & 0x0004) > 0;
    sensorStatus->isInadequateSignalDetected = (status & 0x0008) > 0;
    sensorStatus->isPoorSignalDetected = (status & 0x0010) > 0;
    sensorStatus->isLowPerfusionDetected = (status & 0x0020) > 0;
    sensorStatus->isErraticSignalDetected = (status & 0x0040) > 0;
    sensorStatus->isNonpulsatileSignalDetected = (status & 0x0080) > 0;
    sensorStatus->isQuestionablePulseDetected = (status & 0x0100) > 0;
    sensorStatus->isSignalAnalysisOngoing = (status & 0x0200) > 0;
    sensorStatus->isSensorInterfaceDetected = (status & 0x0400) > 0;
    sensorStatus->isSensorUnconnectedDetected = (status & 0x0800) > 0;
    sensorStatus->isUnknownSensorConnected = (status & 0x1000) > 0;
    sensorStatus->isSensorDisplaced = (status & 0x2000) > 0;
    sensorStatus->isSensorMalfunctioning = (status & 0x4000) > 0;
    sensorStatus->isSensorDisconnected = (status & 0x8000) > 0;
    return sensorStatus;
}

SpotMeasurement *plx_create_spot_measurement(const GByteArray *byteArray) {
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
    measurement->measurement_status = measurementStatusPresent ? plx_create_measurement_status(
            parser_get_uint16(parser)) : NULL;
    measurement->sensor_status = sensorStatusPresent ? plx_create_sensor_status(parser_get_uint16(parser)) : NULL;
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