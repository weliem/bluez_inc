//
// Created by martijn on 22-01-22.
//

#ifndef TEST_GLX_MEASUREMENT_H
#define TEST_GLX_MEASUREMENT_H

#include "../observation_units.h"
#include <glib.h>

typedef struct glx_measurement {
    float glucoseConcentration;
    ObservationUnit unit;
    guint8 measurementType;
    guint8 location;
    GDateTime *timestamp;
    int sequenceNumber;
    gboolean isSensorStatusAnnuciationPresent;
    int sensorStatusAnnuciation;
    gboolean isContextWillFollow;
} GlxMeasurement;

GlxMeasurement *glx_measurement_create(const GByteArray *byteArray);

#endif //TEST_GLX_MEASUREMENT_H
