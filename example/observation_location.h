//
// Created by martijn on 10-11-21.
//

#ifndef TEST_OBSERVATION_LOCATION_H
#define TEST_OBSERVATION_LOCATION_H

typedef enum observation_location {
    LOCATION_UNKNOWN,
    LOCATION_ARM,
    LOCATION_ARMPIT,
    LOCATION_BODY,
    LOCATION_CHEST,
    LOCATION_EAR,
    LOCATION_FOOT,
    LOCATION_FOREHEAD,
    LOCATION_FINGER,
    LOCATION_GASTRO_INTESTINAL_TRACT,
    LOCATION_MOUTH,
    LOCATION_RECTUM,
    LOCATION_TOE,
    LOCATION_TYMPANUM,
    LOCATION_WRIST
} ObservationLocation;

const char* observation_location_str(ObservationLocation unit);

#endif //TEST_OBSERVATION_LOCATION_H
