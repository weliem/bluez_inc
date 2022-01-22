//
// Created by martijn on 22-01-22.
//

#include "glx_feature.h"
#include "../../parser.h"

GlxFeatures *glx_create_features(const GByteArray *byteArray) {
    GlxFeatures *features = g_new0(GlxFeatures, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);
    guint16 flags = parser_get_uint16(parser);

    features->lowBatteryDetectionDuringMeasurementSupported = (flags & 0x0001) > 0;
    features-> isMalfunctionDetectionSupported = (flags & 0x0002) > 0;
    features-> isSampleSizeSupported = (flags & 0x0004) > 0;
    features-> isStripInsertionErrorDetectionSupported = (flags & 0x0008) > 0;
    features-> isStripTypeErrorDetectionSupported = (flags & 0x0010) > 0;
    features-> isResultHighLowDetectionSupported = (flags & 0x0020) > 0;
    features-> isTemperatureHighLowDetectionSupported = (flags & 0x0040) > 0;
    features-> isReadInterruptDetectionSupported = (flags & 0x0080) > 0;
    features-> isDeviceFaultSupported = (flags & 0x0100) > 0;
    features-> isTimeFaultSupported = (flags & 0x0200) > 0;
    features-> isMultiBondSupported = (flags & 0x0400) > 0;

    parser_free(parser);
    return features;
}