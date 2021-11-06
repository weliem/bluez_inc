//
// Created by martijn on 04-11-21.
//

#include "hts_service_handler.h"
#include "characteristic.h"
#include "parser.h"
#include "logger.h"
#include "device.h"

#define TAG "HTS_Service_Handler"

#define HTS_SERVICE_UUID "00001809-0000-1000-8000-00805f9b34fb"
#define TEMPERATURE_CHAR_UUID "00002a1c-0000-1000-8000-00805f9b34fb"

typedef enum observation_unit {
    FAHRENHEIT = 0, CELSIUS = 1
} ObservationUnit;

typedef enum temperature_type {
    ARMPIT = 0, EAR = 1, UNKNOWN = 99
} TemperatureType;

typedef struct hts_measurement {
    float value;
    ObservationUnit unit;
    GDateTime *timestamp;
    TemperatureType temperature_type;
} TemperatureMeasurement;

static TemperatureMeasurement *hts_create_measurement(GByteArray *byteArray) {
    TemperatureMeasurement *measurement = g_new0(TemperatureMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timestamp_present = (flags & 0x02) > 0;
    gboolean type_present = (flags & 0x04) > 0;

    measurement->value = parser_get_float(parser);
    measurement->unit = (flags & 0x01) > 0 ? FAHRENHEIT : CELSIUS;
    measurement->timestamp = parser_get_date_time(parser);
    measurement->temperature_type = type_present ? parser_get_uint8(parser) : UNKNOWN;

    parser_free(parser);
    return measurement;
}

void hts_measurement_free(TemperatureMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    g_free(measurement);
}

static void hts_onCharacteristicsDiscovered(gpointer *handler, Device *device) {
    Characteristic *temperature = binc_device_get_characteristic(device, HTS_SERVICE_UUID, TEMPERATURE_CHAR_UUID);
    if (temperature != NULL && binc_characteristic_supports_notify(temperature)) {
        binc_characteristic_start_notify(temperature);
    }
}

static void hts_onNotificationStateUpdated(gpointer *handler, Device *device, Characteristic *characteristic) {
    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    log_debug(TAG, "characteristic <%s> notifying %s", binc_characteristic_get_uuid(characteristic), is_notifying ? "true" : "false");
}

static void hts_onCharacteristicWrite(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {

}

static void hts_onCharacteristicChanged(gpointer *handler, Device *device, Characteristic *characteristic, GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);


    if (g_str_equal(uuid, TEMPERATURE_CHAR_UUID)) {
        TemperatureMeasurement *measurement = hts_create_measurement(byteArray);

        // Log the measurement
        char* time_string = g_date_time_format(measurement->timestamp, "%F %R:%S");
        log_debug(TAG, "temperature=%.1f, unit=%s, timestamp=%s",
                  measurement->value,
                  measurement->unit == FAHRENHEIT ? "F":"C",
                  time_string
                  );
        g_free(time_string);

        hts_measurement_free(measurement);
    }
}

static void hts_free(gpointer *handler) {
    log_debug(TAG, "freeing HTS handler");
}

ServiceHandler* hts_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->handler = NULL;
    handler->uuid = "00001809-0000-1000-8000-00805f9b34fb";
    handler->on_characteristics_discovered = &hts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &hts_onNotificationStateUpdated;
    handler->on_characteristic_write = &hts_onCharacteristicWrite;
    handler->on_characteristic_changed = &hts_onCharacteristicChanged;
    handler->service_handler_free = &hts_free;
    return handler;
}