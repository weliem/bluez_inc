//
// Created by martijn on 09-11-21.
//

#include "observation_units.h"

const char *observation_unit_names[] = {
        [FAHRENHEIT] = "Fahrenheit",
        [CELSIUS] = "Celsius",
        [MMHG]  = "mmHg",
        [KPA]  = "kPa",
        [BPM]  = "bpm",
        [LBS] = "lbs",
        [KG] = "Kg",
        [PERCENT] = "%"
};

const char* observation_unit_str(ObservationUnit unit) {
    return observation_unit_names[unit];
}


