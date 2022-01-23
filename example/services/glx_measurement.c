//
// Created by martijn on 22-01-22.
//

#include "glx_measurement.h"
#include "../../parser.h"
#include "../observation.h"

GList *glx_measurement_as_observation(GlxMeasurement *measurement) {
    GList *observation_list = NULL;

    Observation *observation = g_new0(Observation, 1);
    observation->value = measurement->glucoseConcentration;
    observation->unit = measurement->unit;
    observation->duration_msec = 5000;
    observation->type = BLOOD_GLUCOSE;
    observation->timestamp = g_date_time_ref(measurement->timestamp);
    observation->received = g_date_time_new_now_local();
    observation->location = LOCATION_FINGER;
    observation_list = g_list_append(observation_list, observation);

    return observation_list;
}
GlxMeasurement *glx_measurement_create(const GByteArray *byteArray) {
    GlxMeasurement *measurement = g_new0(GlxMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timeOffsetPresent = (flags & 0x01) > 0;
    gboolean concentrationTypeAndLocationPresent = (flags & 0x02) > 0;
    measurement->unit = (flags & 0x04) > 0 ? MMOLL : MGDL;
    gboolean isSensorStatusAnnuciationPresent = (flags & 0x08) > 0;
    measurement->isContextWillFollow = (flags & 0x10) > 0;

    measurement->sequenceNumber = parser_get_uint16(parser);
    measurement->timestamp = parser_get_date_time(parser);

    if (timeOffsetPresent) {
        int offset = parser_get_sint16(parser);
        long timestampUTC = g_date_time_to_unix(measurement->timestamp);
        long adjustedTimestampUTC = timestampUTC + offset * 60;
        g_date_time_unref(measurement->timestamp);
        measurement->timestamp = g_date_time_new_from_unix_local(adjustedTimestampUTC);
    }

    if (concentrationTypeAndLocationPresent) {
        int multiplier = measurement->unit == MGDL ? 100000 : 1000;
        measurement->glucoseConcentration = parser_get_sfloat(parser) * multiplier;
        guint8 typeLocation = parser_get_uint8(parser);
        measurement->measurementType = typeLocation & 0x0F;
        measurement->location = (typeLocation & 0xF0) >> 4;
    }

    if(isSensorStatusAnnuciationPresent) {
        measurement->sensorStatusAnnuciation = parser_get_uint16(parser);
    }

    parser_free(parser);
    return measurement;
}

void glx_measurement_free(GlxMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    g_free(measurement);
}