//
// Created by martijn on 10-11-21.
//

#include "observation_location.h"

const char *observation_location_names[] = {
        [LOCATION_ARM] = "Arm",
        [LOCATION_TYMPANUM] = "Tympanum",
        [LOCATION_UNKNOWN]  = "Unknown",
        [LOCATION_EAR]  = "Ear",
        [LOCATION_TOE]  ="Toe",
        [LOCATION_RECTUM] = "Rectum",
        [LOCATION_MOUTH] = "Mouth",
        [LOCATION_GASTRO_INTESTINAL_TRACT] = "GastroIntestinalTract",
        [LOCATION_FINGER] = "Finger",
        [LOCATION_BODY] = "Body",
        [LOCATION_CHEST] = "Chest",
        [LOCATION_FOOT] = "Foot",
        [LOCATION_WRIST] = "Wrist",
        [LOCATION_ARMPIT] = "Armpit",
        [LOCATION_FOREHEAD] = "Forehead"
};

const char* observation_location_str(ObservationLocation unit) {
    return observation_location_names[unit];
}