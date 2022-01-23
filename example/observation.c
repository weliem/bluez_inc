//
// Created by martijn on 09-11-21.
//

#include "observation.h"
#include "cJSON.h"
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

static void add_observation(Observation *observation, cJSON *fhir_json) {
    cJSON *code = cJSON_AddObjectToObject(fhir_json, "code");
    cJSON *coding_loinc = cJSON_CreateObject();
    cJSON_AddStringToObject(coding_loinc, "system", "http://loinc.org");
    cJSON_AddStringToObject(coding_loinc, "code", observation_get_loinc_code(observation));
    cJSON_AddStringToObject(coding_loinc, "display", observation_get_display_str(observation));
    cJSON *coding_array = cJSON_CreateArray();
    cJSON_AddItemToArray(coding_array, coding_loinc);
    cJSON_AddItemToObject(code, "coding", coding_array);

    // Build up value
    cJSON *value = cJSON_AddObjectToObject(fhir_json, "valueQuantity");
    const char *precision = observation_precision[observation->type];
    char *precision_string = g_strdup_printf("%%.%sf", precision);
    char *value_string = g_strdup_printf(precision_string,
                                         binc_round_with_precision(observation->value, atoi(precision)));
    cJSON_AddRawToObject(value, "value", value_string);
    cJSON_AddStringToObject(value, "unit", observation_unit_str(observation->unit));
    cJSON_AddStringToObject(value, "system", "http://unitsofmeasure.org");
    cJSON_AddStringToObject(value, "code", observation_get_unit_ucum_str(observation));
    g_free(precision_string);
    g_free(value_string);
}

cJSON *observation_list_as_fhir(GList *observation_list, const char *device_reference) {
    g_assert(observation_list != NULL);
    if (g_list_length(observation_list) == 0) return NULL;

    cJSON *fhir_json = cJSON_CreateObject();
    cJSON_AddStringToObject(fhir_json, "resourceType", "Observation");
    cJSON_AddStringToObject(fhir_json, "status", "final");

    // Build up vital signs category
    cJSON *category = cJSON_AddArrayToObject(fhir_json, "category");
    cJSON *category_item = cJSON_CreateObject();
    cJSON *category_entry = cJSON_CreateObject();
    cJSON *category_coding = cJSON_AddArrayToObject(category_item, "coding");
    cJSON_AddStringToObject(category_entry, "system", "http://terminology.hl7.org/CodeSystems/observation_category");
    cJSON_AddStringToObject(category_entry, "code", "vital-signs");
    cJSON_AddStringToObject(category_entry, "display", "Vital Signs");
    cJSON_AddItemToArray(category_coding, category_entry);
    cJSON_AddStringToObject(category_item, "text", "Vital Signs");
    cJSON_AddItemToArray(category, category_item);


    // Build up coding
    Observation *first_observation = (Observation *) observation_list->data;
    if (g_list_length(observation_list) > 1) {
        // Only supporting Blood pressure for now
        cJSON *code = cJSON_AddObjectToObject(fhir_json, "code");
        cJSON *coding_loinc = cJSON_CreateObject();
        cJSON_AddStringToObject(coding_loinc, "system", "http://loinc.org");
        cJSON_AddStringToObject(coding_loinc, "code", "35094-2");   // Or 85354-9????
        cJSON_AddStringToObject(coding_loinc, "display", "Blood pressure panel");
        cJSON *coding_array = cJSON_CreateArray();
        cJSON_AddItemToArray(coding_array, coding_loinc);
        cJSON_AddItemToObject(code, "coding", coding_array);

        cJSON *component = cJSON_AddArrayToObject(fhir_json, "component");
        for (GList *iterator = observation_list; iterator; iterator = iterator->next) {
            Observation *observation = (Observation *) iterator->data;
            cJSON *single_observation = cJSON_CreateObject();
            cJSON_AddItemToArray(component, single_observation);
            add_observation(observation, single_observation);
        }
    } else {
        add_observation(first_observation, fhir_json);
    }

    char *time_string = binc_date_time_format_iso8601(first_observation->timestamp);
    cJSON_AddStringToObject(fhir_json, "effectiveDateTime", time_string);
    g_free(time_string);

    if (device_reference != NULL) {
        cJSON *device_ref = cJSON_CreateObject();
        cJSON_AddStringToObject(device_ref, "reference", device_reference);
        cJSON_AddItemToObject(fhir_json, "device", device_ref);
    }

    return fhir_json;
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