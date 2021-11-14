//
// Created by martijn on 09-11-21.
//

#include "observation.h"
#include "cJSON.h"

const char *observation_type_loinc_codes[] = {
        [BLOOD_PRESSURE_SYSTOLIC] = "8480-6",
        [BLOOD_PRESSURE_DIASTOLIC] = "8462-4",
        [BLOOD_PRESSURE_MEAN]  = "8478-0",
        [BLOOD_PRESSURE_PULSE]  = "8867-4",
        [BODY_TEMPERATURE]  = "8310-5"
};

const char *observation_type_loinc_display[] = {
        [BLOOD_PRESSURE_SYSTOLIC] = "Systolic blood pressure",
        [BLOOD_PRESSURE_DIASTOLIC] = "Diastolic blood pressure",
        [BLOOD_PRESSURE_MEAN]  = "Mean arterial blood pressure",
        [BLOOD_PRESSURE_PULSE]  = "Blood pressure pulse",
        [BODY_TEMPERATURE]  = "Body temperature"
};

const char *observation_unit_ucum[] = {
        [CELSIUS] = "Cel",
        [FAHRENHEIT] =  "[degF]",
        [MMHG] = "mm[Hg]",
        [KPA] = "kPa",
        [BPM] = "/min"
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
    cJSON_AddNumberToObject(value, "value", observation->value);
    cJSON_AddStringToObject(value, "unit", observation_unit_str(observation->unit));
    cJSON_AddStringToObject(value, "system", "http://unitsofmeasure.org");
    cJSON_AddStringToObject(value, "code", observation_get_unit_ucum_str(observation));
}

char *observation_list_as_fhir(GList *observation_list) {
    cJSON *fhir_json = cJSON_CreateObject();
    if (fhir_json == NULL) return NULL;

    cJSON_AddStringToObject(fhir_json, "resourceType", "Observation");
    cJSON_AddStringToObject(fhir_json, "status", "final");

    // Build up vital signs category
    cJSON *category = cJSON_AddArrayToObject(fhir_json, "category");
    cJSON *category_entry = cJSON_CreateObject();
    cJSON_AddStringToObject(category_entry, "system", "http://terminology.hl7.org/CodeSystems/observation_category");
    cJSON_AddStringToObject(category_entry, "code", "vital-signs");
    cJSON_AddStringToObject(category_entry, "display", "Vital Signs");
    cJSON_AddItemToArray(category, category_entry);

    // Build up coding
    Observation *first_observation = (Observation *) observation_list->data;
    if (g_list_length(observation_list) > 1) {
        // Only supporting Blood pressure for now
        cJSON *code = cJSON_AddObjectToObject(fhir_json, "code");
        cJSON *coding_loinc = cJSON_CreateObject();
        cJSON_AddStringToObject(coding_loinc, "system", "http://loinc.org");
        cJSON_AddStringToObject(coding_loinc, "code", "35094-2");
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

    char *time_string = g_date_time_format_iso8601(first_observation->timestamp);
    cJSON_AddStringToObject(fhir_json, "effectiveDateTime", time_string);

    char *result = cJSON_Print(fhir_json);
    cJSON_Delete(fhir_json);
    g_free(time_string);
    return result;
}

void observation_list_free(GList *observation_list) {
    for (GList *iterator = observation_list; iterator; iterator = iterator->next) {
        Observation *observation = (Observation *) iterator->data;
        if (observation->received != NULL) {
            g_date_time_unref(observation->received);
        }
        if (observation->timestamp != NULL) {
            g_date_time_unref(observation->timestamp);
        }
        g_free(observation);
    }
    g_list_free(observation_list);
}