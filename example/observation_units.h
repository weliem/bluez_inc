//
// Created by martijn on 07-11-21.
//

#ifndef TEST_OBSERVATION_UNITS_H
#define TEST_OBSERVATION_UNITS_H

typedef enum observation_unit {
    FAHRENHEIT = 0,
    CELSIUS = 1,
    MMHG = 2,
    KPA = 3,
    BPM = 4,
    LBS = 5,
    KG = 6,
    PERCENT = 7,
    MMOLL = 8,
    MGDL = 9
} ObservationUnit;

const char* observation_unit_str(ObservationUnit unit);


#endif //TEST_OBSERVATION_UNITS_H
