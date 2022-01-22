//
// Created by martijn on 22-01-22.
//

#ifndef TEST_GLX_FEATURE_H
#define TEST_GLX_FEATURE_H

#include <glib.h>

typedef struct glx_features {
    gboolean lowBatteryDetectionDuringMeasurementSupported;
    gboolean isMalfunctionDetectionSupported;
    gboolean isSampleSizeSupported;
    gboolean isStripInsertionErrorDetectionSupported;
    gboolean isStripTypeErrorDetectionSupported;
    gboolean isResultHighLowDetectionSupported;
    gboolean isTemperatureHighLowDetectionSupported;
    gboolean isReadInterruptDetectionSupported;
    gboolean isDeviceFaultSupported;
    gboolean isTimeFaultSupported;
    gboolean isMultiBondSupported;
} GlxFeatures;

GlxFeatures *glx_create_features(const GByteArray *byteArray);

#endif //TEST_GLX_FEATURE_H
