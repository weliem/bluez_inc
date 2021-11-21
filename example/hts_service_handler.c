//
// Created by martijn on 04-11-21.
//

#include "hts_service_handler.h"
#include "../characteristic.h"
#include "../parser.h"
#include "../logger.h"
#include "../device.h"
#include "observation_units.h"
#include "observation.h"
#include "observation_location.h"

#define TAG "HTS_Service_Handler"


typedef enum temperature_type {
    TT_UNKNOWN = 0, TT_ARMPIT = 1, TT_BODY = 2, TT_EAR = 3, TT_FINGER = 4, TT_GASTRO_INTESTINAL_TRACT = 5,
    TT_MOUTH = 6, TT_RECTUM = 7, TT_TOE = 8, TT_TYMPANUM = 9, TT_FOREHEAD = 10
} TemperatureType;

typedef struct hts_measurement {
    float value;
    ObservationUnit unit;
    GDateTime *timestamp;
    TemperatureType temperature_type;
} TemperatureMeasurement;

static ObservationLocation hts_get_location(TemperatureMeasurement *measurement) {
    switch(measurement->temperature_type) {
        case TT_UNKNOWN: return LOCATION_UNKNOWN;
        case TT_ARMPIT: return LOCATION_ARMPIT;
        case TT_BODY : return LOCATION_BODY;
        case TT_EAR : return LOCATION_EAR;
        case TT_FINGER : return LOCATION_FINGER;
        case TT_GASTRO_INTESTINAL_TRACT : return LOCATION_GASTRO_INTESTINAL_TRACT;
        case TT_MOUTH : return LOCATION_MOUTH;
        case TT_RECTUM : return LOCATION_RECTUM;
        case TT_TOE : return LOCATION_TOE;
        case TT_TYMPANUM : return LOCATION_TYMPANUM;
        case TT_FOREHEAD : return LOCATION_FOREHEAD;
        default: return LOCATION_UNKNOWN;
    }
}
static Observation *hts_measurement_as_observation(TemperatureMeasurement *measurement) {
    Observation *observation = g_new0(Observation, 1);
    observation->value = measurement->value;
    observation->unit = measurement->unit;
    observation->duration_msec = 1000;
    observation->type = BODY_TEMPERATURE;
    observation->timestamp = g_date_time_ref(measurement->timestamp);
    observation->received = g_date_time_new_now_local();
    observation->location = hts_get_location(measurement);
    return observation;
}

static TemperatureMeasurement *hts_create_measurement(GByteArray *byteArray) {
    TemperatureMeasurement *measurement = g_new0(TemperatureMeasurement, 1);
    Parser *parser = parser_create(byteArray, LITTLE_ENDIAN);

    guint8 flags = parser_get_uint8(parser);
    gboolean timestamp_present = (flags & 0x02) > 0;
    gboolean type_present = (flags & 0x04) > 0;

    measurement->value = parser_get_float(parser);
    measurement->unit = (flags & 0x01) > 0 ? FAHRENHEIT : CELSIUS;
    measurement->timestamp = timestamp_present ? parser_get_date_time(parser) : NULL;
    measurement->temperature_type = type_present ? parser_get_uint8(parser) : TT_UNKNOWN;

    parser_free(parser);
    return measurement;
}

void hts_measurement_free(TemperatureMeasurement *measurement) {
    g_date_time_unref(measurement->timestamp);
    measurement->timestamp = NULL;
    g_free(measurement);
}

static void hts_onCharacteristicsDiscovered(ServiceHandler *service_handler, Device *device) {
    log_debug(TAG, "discovered HealthThermometerService");
    Characteristic *temperature = binc_device_get_characteristic(device, HTS_SERVICE_UUID, TEMPERATURE_CHAR_UUID);
    if (temperature != NULL && binc_characteristic_supports_notify(temperature)) {
        binc_characteristic_start_notify(temperature);
    }
}

static void hts_onNotificationStateUpdated(ServiceHandler *service_handler, Device *device,
                                           Characteristic *characteristic,GError *error) {

    const char *uuid = binc_characteristic_get_uuid(characteristic);
    gboolean is_notifying = binc_characteristic_is_notifying(characteristic);
    if (error != NULL) {
        log_debug(TAG, "failed to start/stop notify '%s' (error %d: %s)", uuid, error->code, error->message);
        return;
    }
    log_debug(TAG, "characteristic <%s> notifying %s", uuid, is_notifying ? "true" : "false");
}

static void hts_onCharacteristicWrite(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                      GByteArray *byteArray, GError *error) {

}

static void hts_onCharacteristicChanged(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                        GByteArray *byteArray, GError *error) {
    const char *uuid = binc_characteristic_get_uuid(characteristic);
    GList *observation_list = NULL;

    if (g_str_equal(uuid, TEMPERATURE_CHAR_UUID)) {
        TemperatureMeasurement *measurement = hts_create_measurement(byteArray);

        // Device specific corrections
        const char* device_name = binc_device_get_name(device);
        if (g_str_has_prefix(device_name, "Philips ear thermometer")) {
            measurement->temperature_type = TT_TYMPANUM;
        } else if (g_str_has_prefix(device_name, "TAIDOC TD1242")) {
            measurement->temperature_type = TT_FOREHEAD;
        }

        Observation *observation = hts_measurement_as_observation(measurement);
        hts_measurement_free(measurement);

        observation_list = g_list_append(observation_list, observation);
        if (service_handler->observations_callback != NULL) {
            service_handler->observations_callback(observation_list);
        }
        observation_list_free(observation_list);
    }
}

static void hts_free(ServiceHandler *service_handler) {
    log_debug(TAG, "freeing HTS private_data");
}

ServiceHandler *hts_service_handler_create() {
    ServiceHandler *handler = g_new0(ServiceHandler, 1);
    handler->private_data = NULL;
    handler->uuid = HTS_SERVICE_UUID;
    handler->on_characteristics_discovered = &hts_onCharacteristicsDiscovered;
    handler->on_notification_state_updated = &hts_onNotificationStateUpdated;
    handler->on_characteristic_write = &hts_onCharacteristicWrite;
    handler->on_characteristic_changed = &hts_onCharacteristicChanged;
    handler->service_handler_free = &hts_free;
    return handler;
}