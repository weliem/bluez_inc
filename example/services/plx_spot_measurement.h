//
// Created by martijn on 16-01-22.
//

#ifndef TEST_PLX_SPOT_MEASUREMENT_H
#define TEST_PLX_SPOT_MEASUREMENT_H

#include <glib.h>

typedef struct plx_sensor_status {
    gboolean isExtendedDisplayUpdateOngoing;
    gboolean isEquipmentMalfunctionDetected;
    gboolean isSignalProcessingIrregularityDetected;
    gboolean isInadequateSignalDetected;
    gboolean isPoorSignalDetected;
    gboolean isLowPerfusionDetected;
    gboolean isErraticSignalDetected;
    gboolean isNonpulsatileSignalDetected;
    gboolean isQuestionablePulseDetected;
    gboolean isSignalAnalysisOngoing;
    gboolean isSensorInterfaceDetected;
    gboolean isSensorUnconnectedDetected;
    gboolean isUnknownSensorConnected;
    gboolean isSensorDisplaced;
    gboolean isSensorMalfunctioning;
    gboolean isSensorDisconnected;
} SensorStatus;

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
    MeasurementStatus *measurement_status;
    SensorStatus *sensor_status;
    GDateTime *timestamp;
    gboolean is_device_clock_set;
} SpotMeasurement;

SpotMeasurement *plx_create_spot_measurement(const GByteArray *byteArray);

void plx_spot_measurement_free(SpotMeasurement *measurement);

GList *plx_spot_measurement_as_observations(SpotMeasurement *measurement);

#endif //TEST_PLX_SPOT_MEASUREMENT_H
