//
// Created by martijn on 24-11-21.
//

#include <glib.h>
#include "device_info.h"
#include "cJSON.h"

static GHashTable *device_info_store;

struct device_info {
    char *address;
    char *manufacturer;
    char *model;
    char *serial_number;
    char *firmware_version;
    char *software_version;
    char *hardware_version;
    guint8 battery_level;
    GDateTime *device_time;
};

DeviceInfo *get_device_info(const char *address) {
    g_assert(address != NULL);

    if (device_info_store == NULL) {
        device_info_store = g_hash_table_new(g_str_hash, g_str_equal);
    }

    DeviceInfo *result = g_hash_table_lookup(device_info_store, address);
    if (result == NULL) {
        result = g_new0(DeviceInfo, 1);
        result->address = g_strdup(address);
        g_hash_table_insert(device_info_store, g_strdup(address), result);
    }
    return result;
}

void device_info_free(DeviceInfo *deviceInfo) {
    if (deviceInfo->address != NULL) {
        g_free(deviceInfo->address);
        deviceInfo->address = NULL;
    }

    if (deviceInfo->manufacturer != NULL) {
        g_free(deviceInfo->manufacturer);
        deviceInfo->manufacturer = NULL;
    }

    if (deviceInfo->model != NULL) {
        g_free(deviceInfo->model);
        deviceInfo->model = NULL;
    }

    if (deviceInfo->serial_number != NULL) {
        g_free(deviceInfo->serial_number);
        deviceInfo->serial_number = NULL;
    }

    if (deviceInfo->firmware_version != NULL) {
        g_free(deviceInfo->firmware_version);
        deviceInfo->firmware_version = NULL;
    }

    if (deviceInfo->software_version != NULL) {
        g_free(deviceInfo->software_version);
        deviceInfo->software_version = NULL;
    }

    if (deviceInfo->hardware_version != NULL) {
        g_free(deviceInfo->hardware_version);
        deviceInfo->hardware_version = NULL;
    }

    if (deviceInfo->device_time != NULL) {
        g_date_time_unref(deviceInfo->device_time);
    }

    g_free(deviceInfo);
}

void device_info_close() {
    if (device_info_store != NULL) {
        GHashTableIter iter;
        gpointer key, value;
        g_hash_table_iter_init(&iter, device_info_store);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            g_free(key);
            DeviceInfo *deviceInfo = (DeviceInfo *) value;
            device_info_free(deviceInfo);
        }
        g_hash_table_destroy(device_info_store);
        device_info_store = NULL;
    }
}

void device_info_set_manufacturer(DeviceInfo *deviceInfo, const char *manufacturer) {
    g_assert(manufacturer != NULL);
    if (deviceInfo->manufacturer != NULL) {
        g_free(deviceInfo->manufacturer);
    }
    deviceInfo->manufacturer = g_strdup(manufacturer);
}

void device_info_set_model(DeviceInfo *deviceInfo, const char *model) {
    g_assert(model != NULL);
    if (deviceInfo->model != NULL) {
        g_free(deviceInfo->model);
    }
    deviceInfo->model = g_strdup(model);
}

void device_info_set_serialnumber(DeviceInfo *deviceInfo, const char *serialnumber) {
    g_assert(serialnumber != NULL);
    if (deviceInfo->serial_number != NULL) {
        g_free(deviceInfo->serial_number);
    }
    deviceInfo->serial_number = g_strdup(serialnumber);
}

void device_info_set_firmware_version(DeviceInfo *deviceInfo, const char *firmware_version) {
    g_assert(firmware_version != NULL);
    if (deviceInfo->firmware_version != NULL) {
        g_free(deviceInfo->firmware_version);
    }
    deviceInfo->firmware_version = g_strdup(firmware_version);
}

const char *device_info_get_address(DeviceInfo *deviceInfo) {
    g_assert(deviceInfo != NULL);
    return deviceInfo->address;
}

const char *device_info_get_manufacturer(DeviceInfo *deviceInfo) {
    g_assert(deviceInfo != NULL);
    return deviceInfo->manufacturer;
}

const char *device_info_get_model(DeviceInfo *deviceInfo) {
    g_assert(deviceInfo != NULL);
    return deviceInfo->model;
}

const char *device_info_get_serialnumber(DeviceInfo *deviceInfo) {
    g_assert(deviceInfo != NULL);
    return deviceInfo->serial_number;
}

const char *device_info_get_firmware_version(DeviceInfo *deviceInfo) {
    g_assert(deviceInfo != NULL);
    return deviceInfo->firmware_version;
}

cJSON *device_info_to_fhir(DeviceInfo *deviceInfo) {
    cJSON *fhir_json = cJSON_CreateObject();
    if (fhir_json == NULL) return NULL;

    cJSON_AddStringToObject(fhir_json, "resourceType", "Device");
    cJSON_AddStringToObject(fhir_json, "status", "active");
    cJSON *identifier_array = cJSON_AddArrayToObject(fhir_json, "identifier");
    cJSON *identifier = cJSON_CreateObject();
    cJSON_AddStringToObject(identifier, "system", DEVICE_SYSTEM);

    GString *mac_address = g_string_new(deviceInfo->address);
    g_string_replace(mac_address, ":", "-", 0);
    cJSON_AddStringToObject(identifier, "value", mac_address->str);
    g_string_free(mac_address, TRUE);
    cJSON_AddItemToArray(identifier_array, identifier);

    if (deviceInfo->manufacturer != NULL) {
        cJSON_AddStringToObject(fhir_json, "manufacturer", deviceInfo->manufacturer);
    }

    if (deviceInfo->model != NULL) {
        cJSON_AddStringToObject(fhir_json, "modelNumber", deviceInfo->model);
    }

    if (deviceInfo->serial_number != NULL) {
        cJSON_AddStringToObject(fhir_json, "serialNumber", deviceInfo->serial_number);
    }

    return fhir_json;
//    char *result = cJSON_Print(fhir_json);
//    cJSON_Delete(fhir_json);
//    return result;
}