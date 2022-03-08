//
// Created by martijn on 09-11-21.
//

#include "observation.h"
#include "../utility.h"

const char *observation_type_loinc_codes[] = {
        [BLOOD_PRESSURE_SYSTOLIC] = "8480-6",
        [BLOOD_PRESSURE_DIASTOLIC] = "8462-4",
        [BLOOD_PRESSURE_MEAN]  = "8478-0",
        [BLOOD_PRESSURE_PULSE]  = "8867-4",
        [BODY_TEMPERATURE]  = "8310-5",
        [BODY_WEIGHT] = "3141-9",
        [BLOOD_OXYGEN_SATURATION] = "59408-5",
        [PULSE_OXIMETRY_PULSE] = "8889-8",
        [BLOOD_GLUCOSE] = "2339-0"
};

const char *observation_type_loinc_display[] = {
        [BLOOD_PRESSURE_SYSTOLIC] = "Systolic blood pressure",
        [BLOOD_PRESSURE_DIASTOLIC] = "Diastolic blood pressure",
        [BLOOD_PRESSURE_MEAN]  = "Mean arterial blood pressure",
        [BLOOD_PRESSURE_PULSE]  = "Blood pressure pulse",
        [BODY_TEMPERATURE]  = "Body temperature",
        [BODY_WEIGHT] = "Body weight measured",
        [BLOOD_OXYGEN_SATURATION] = "Blood oxygen saturation in arterial blood",
        [PULSE_OXIMETRY_PULSE] = "Heart rate by Pulse oximetry",
        [BLOOD_GLUCOSE] = "Glucose [Mass/volume] in Blood"
};

const char *observation_precision[] = {
        [BLOOD_PRESSURE_SYSTOLIC] = "0",
        [BLOOD_PRESSURE_DIASTOLIC] = "0",
        [BLOOD_PRESSURE_MEAN]  = "0",
        [BLOOD_PRESSURE_PULSE]  = "0",
        [BODY_TEMPERATURE]  = "1",
        [BODY_WEIGHT] = "1",
        [BLOOD_OXYGEN_SATURATION] = "0",
        [PULSE_OXIMETRY_PULSE] = "0",
        [BLOOD_GLUCOSE] = "1"
};

const char *observation_unit_ucum[] = {
        [CELSIUS] = "Cel",
        [FAHRENHEIT] =  "[degF]",
        [MMHG] = "mm[Hg]",
        [KPA] = "kPa",
        [BPM] = "/min",
        [KG] = "kg",
        [LBS] = "[lb_av]",
        [PERCENT] = "%",
        [MMOLL] = "mmol/L",
        [MGDL] = "mg/dL"
};

const char *observation_get_loinc_code(Observation *observation) {
    return observation_type_loinc_codes[observation->type];
}

const char *observation_get_display_str(Observation *observation) {
    return observation_type_loinc_display[observation->type];
}

const char *observation_get_unit_ucum_str(Observation *observation) {
    return observation_unit_ucum[observation->unit];
}


static void observation_free(Observation *observation) {
    if (observation->received != NULL) {
        g_date_time_unref(observation->received);
    }
    if (observation->timestamp != NULL) {
        g_date_time_unref(observation->timestamp);
    }
    g_free(observation);
}

void observation_list_free(GList *observation_list) {
    g_list_free_full(observation_list, (GDestroyNotify) observation_free);
}