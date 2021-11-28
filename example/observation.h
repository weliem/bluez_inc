//
// Created by martijn on 09-11-21.
//

#ifndef TEST_OBSERVATION_H
#define TEST_OBSERVATION_H

#include <glib.h>
#include "observation_units.h"
#include "observation_location.h"
#include "cJSON.h"


typedef enum observation_type {
    BODY_TEMPERATURE = 0,
    BLOOD_PRESSURE_SYSTOLIC = 1,
    BLOOD_PRESSURE_DIASTOLIC = 2,
    BLOOD_PRESSURE_MEAN = 3,
    BLOOD_PRESSURE_PULSE = 4
} ObservationType;

typedef struct observation {
    float value;
    ObservationUnit unit;
    ObservationType type;
    int duration_msec;
    GDateTime *timestamp;
    GDateTime *received;
    ObservationLocation location;
    int sensor;
} Observation;

const char *observation_get_display_str(Observation *observation);
void observation_list_free(GList *observation_list);
cJSON* observation_list_as_fhir(GList *observation_list, const char* device_reference);

#endif //TEST_OBSERVATION_H
