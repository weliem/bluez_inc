//
// Created by martijn on 16-01-22.
//

#include "plx_features.h"
#include "../../parser.h"


static SensorStatusSupport *plx_create_sensor_status_support(guint16 flags) {
    SensorStatusSupport *support = g_new0(SensorStatusSupport, 1);
    support->isExtendedDisplayUpdateOngoingSupported = (flags & 0x0001) > 0;
    support->isEquipmentMalfunctionDetectedSupported = (flags & 0x0002) > 0;
    support->isSignalProcessingIrregularityDetectedSupported = (flags & 0x0004) > 0;
    support->isInadequateSignalDetectedSupported = (flags & 0x0008) > 0;
    support->isPoorSignalDetectedSupported = (flags & 0x0010) > 0;
    support->isLowPerfusionDetectedSupported = (flags & 0x0020) > 0;
    support->isErraticSignalDetectedSupported = (flags & 0x0040) > 0;
    support->isNonpulsatileSignalDetectedSupported = (flags & 0x0080) > 0;
    support->isQuestionablePulseDetectedSupported = (flags & 0x0100) > 0;
    support->isSignalAnalysisOngoingSupported = (flags & 0x0200) > 0;
    support->isSensorInterfaceDetectedSupported = (flags & 0x0400) > 0;
    support->isSensorUnconnectedDetectedSupported = (flags & 0x0800) > 0;
    support->isUnknownSensorConnectedSupported = (flags & 0x1000) > 0;
    support->isSensorDisplacedSupported = (flags & 0x2000) > 0;
    support->isSensorMalfunctioningSupported = (flags & 0x4000) > 0;
    support->isSensorDisconnectedSupported = (flags & 0x8000) > 0;
    return support;
}

static MeasurementStatusSupport *plx_create_measurement_status_support(guint16 flags) {
    MeasurementStatusSupport *support = g_new0(MeasurementStatusSupport, 1);
    support->isMeasurementOngoingSupported = (flags & 0x0020) > 0;
    support->isEarlyEstimateDataSupported = (flags & 0x0040) > 0;
    support->isValidatedDataSupported = (flags & 0x0080) > 0;
    support->isFullyQualifiedDataSupported = (flags & 0x0100) > 0;
    support->isDataMeasurementStorageSupported = (flags & 0x0200) > 0;
    support->isDataDemonstrationSupported = (flags & 0x0400) > 0;
    support->isDataTestingSupported = (flags & 0x0800) > 0;
    support->isCalibrationOngoingSupported = (flags & 0x1000) > 0;
    support->isMeasurementUnavailableSupported = (flags & 0x2000) > 0;
    support->isQuestionableMeasurementDetectedSupported = (flags & 0x4000) > 0;
    support->isInvalidMeasurementDetectedSupported = (flags & 0x8000) > 0;
    return support;
}

PlxFeatures *plx_create_features(const GByteArray *byteArray) {
    PlxFeatures *features = g_new0(PlxFeatures, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint16 flags = parser_get_uint16(parser);
    features->isMeasurementStatusSupportPresent = (flags & 0x0001) > 0;
    features->isSensorStatusSupportPresent = (flags & 0x0002) > 0;
    features->isSpotMeasurementStorageSupported = (flags & 0x0004) > 0;
    features->isSpotMeasurementTimestampSupported = (flags & 0x0008) > 0;
    features->isSpo2prFastSupported = (flags & 0x0010) > 0;
    features->isSpo2prSlowSupported = (flags & 0x0020) > 0;
    features->isPulseAmplitudeIndexSupported = (flags & 0x0040) > 0;
    features->isMultiBondSupported = (flags & 0x0080) > 0;
    if (features->isMeasurementStatusSupportPresent) {
        guint16 measurement_status_support_flags = parser_get_uint16(parser);
        features->measurementStatusSupport = plx_create_measurement_status_support(measurement_status_support_flags);
    }
    if (features->isSensorStatusSupportPresent) {
        guint16 sensor_status_support_flags = parser_get_uint16(parser);
        features->sensorStatusSupport = plx_create_sensor_status_support(sensor_status_support_flags);
    }
    parser_free(parser);
    return features;
}

void plx_free_features(PlxFeatures *features) {
    if (features->measurementStatusSupport != NULL) {
        g_free(features->measurementStatusSupport);
    }
    if (features->sensorStatusSupport != NULL) {
        g_free(features->sensorStatusSupport);
    }
    g_free(features);
}
