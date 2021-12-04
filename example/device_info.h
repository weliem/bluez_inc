//
// Created by martijn on 24-11-21.
//

#ifndef TEST_DEVICE_INFO_H
#define TEST_DEVICE_INFO_H


#define DEVICE_SYSTEM "http://hl7.org/fhir/sid/eui-48"

#include <glibconfig.h>
#include <glib/gdatetime.h>
#include "cJSON.h"

typedef struct device_info DeviceInfo;

DeviceInfo *get_device_info(const char *address);

void device_info_free(DeviceInfo *deviceInfo);

void device_info_close();

void device_info_set_manufacturer(DeviceInfo *deviceInfo, const char *manufacturer);

void device_info_set_model(DeviceInfo *deviceInfo, const char *model);

void device_info_set_serialnumber(DeviceInfo *deviceInfo, const char *serialnumber);

void device_info_set_firmware_version(DeviceInfo *deviceInfo, const char *firmware_version);
void device_info_set_hardware_version(DeviceInfo *deviceInfo, const char *hardware_version);
void device_info_set_software_version(DeviceInfo *deviceInfo, const char *software_version);

void device_info_set_device_time(DeviceInfo *deviceInfo, GDateTime *device_time);

const char *device_info_get_address(DeviceInfo *deviceInfo);

const char *device_info_get_manufacturer(DeviceInfo *deviceInfo);

const char *device_info_get_model(DeviceInfo *deviceInfo);

const char *device_info_get_serialnumber(DeviceInfo *deviceInfo);

const char *device_info_get_firmware_version(DeviceInfo *deviceInfo);

const char *device_info_get_hardware_version(DeviceInfo *deviceInfo);

const char *device_info_get_software_version(DeviceInfo *deviceInfo);

GDateTime *device_info_get_device_time(DeviceInfo *deviceInfo);

cJSON *device_info_to_fhir(DeviceInfo *deviceInfo);

#endif //TEST_DEVICE_INFO_H
