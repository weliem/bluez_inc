//
// Created by martijn on 09-11-21.
//

#ifndef TEST_OBSERVATION_H
#define TEST_OBSERVATION_H

#include <glib.h>
#include "observation_units.h"
#include "observation_location.h"

typedef struct observation {
    float value;
    ObservationUnit unit;
    const char* type;
    int duration_msec;
    GDateTime *timestamp;
    GDateTime *received;
    ObservationLocation location;
    int sensor;
} Observation;



#endif //TEST_OBSERVATION_H
