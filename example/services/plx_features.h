//
// Created by martijn on 16-01-22.
//

#ifndef TEST_PLX_FEATURES_H
#define TEST_PLX_FEATURES_H

#include <glib.h>

typedef struct plx_sensor_status_support {
    gboolean isExtendedDisplayUpdateOngoingSupported;
    gboolean isEquipmentMalfunctionDetectedSupported;
    gboolean isSignalProcessingIrregularityDetectedSupported;
    gboolean isInadequateSignalDetectedSupported;
    gboolean isPoorSignalDetectedSupported;
    gboolean isLowPerfusionDetectedSupported;
    gboolean isErraticSignalDetectedSupported;
    gboolean isNonpulsatileSignalDetectedSupported;
    gboolean isQuestionablePulseDetectedSupported;
    gboolean isSignalAnalysisOngoingSupported;
    gboolean isSensorInterfaceDetectedSupported;
    gboolean isSensorUnconnectedDetectedSupported;
    gboolean isUnknownSensorConnectedSupported;
    gboolean isSensorDisplacedSupported;
    gboolean isSensorMalfunctioningSupported;
    gboolean isSensorDisconnectedSupported;
} SensorStatusSupport;

typedef struct plx_measurement_status_support {
    gboolean isMeasurementOngoingSupported;
    gboolean isEarlyEstimateDataSupported;
    gboolean isValidatedDataSupported;
    gboolean isFullyQualifiedDataSupported;
    gboolean isDataMeasurementStorageSupported;
    gboolean isDataDemonstrationSupported;
    gboolean isDataTestingSupported;
    gboolean isCalibrationOngoingSupported;
    gboolean isMeasurementUnavailableSupported;
    gboolean isQuestionableMeasurementDetectedSupported;
    gboolean isInvalidMeasurementDetectedSupported;
} MeasurementStatusSupport;

typedef struct plx_features {
    gboolean isMeasurementStatusSupportPresent;
    gboolean isSensorStatusSupportPresent;
    gboolean isSpotMeasurementStorageSupported;
    gboolean isSpotMeasurementTimestampSupported;
    gboolean isSpo2prFastSupported;
    gboolean isSpo2prSlowSupported;
    gboolean isPulseAmplitudeIndexSupported;
    gboolean isMultiBondSupported;
    MeasurementStatusSupport *measurementStatusSupport;
    SensorStatusSupport *sensorStatusSupport;
} PlxFeatures;

PlxFeatures *plx_create_features(const GByteArray *byteArray);

void plx_free_features(PlxFeatures *features);

#endif //TEST_PLX_FEATURES_H
