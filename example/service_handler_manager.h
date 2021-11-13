//
// Created by martijn on 04-11-21.
//

#ifndef TEST_SERVICE_HANDLER_MANAGER_H
#define TEST_SERVICE_HANDLER_MANAGER_H

#include <glib.h>
#include "../forward_decl.h"

typedef struct service_handler_entry ServiceHandler;

typedef void (*onCharacteristicsDiscovered)(ServiceHandler *service_handler, Device *device);

typedef void (*onNotificationStateUpdated)(ServiceHandler *service_handler, Device *device, Characteristic *characteristic, GError *error);

typedef void (*onCharacteristicWrite)(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                      GByteArray *byteArray, GError *error);

typedef void (*onCharacteristicChanged)(ServiceHandler *service_handler, Device *device, Characteristic *characteristic,
                                        GByteArray *byteArray, GError *error);

typedef void (*serviceHandlerFree)(ServiceHandler *service_handler);

typedef void (*observationsCallback)(GList *observations);

struct service_handler_entry {
    gpointer private_data;
    const char *uuid;
    onCharacteristicsDiscovered on_characteristics_discovered;
    onNotificationStateUpdated on_notification_state_updated;
    onCharacteristicWrite on_characteristic_write;
    onCharacteristicChanged on_characteristic_changed;
    serviceHandlerFree service_handler_free;
    observationsCallback observations_callback;
};

ServiceHandlerManager *binc_service_handler_manager_create();

void binc_service_handler_manager_free(ServiceHandlerManager *serviceHandlerManager);

void binc_service_handler_manager_add(ServiceHandlerManager *serviceHandlerManager, ServiceHandler *service_handler);

ServiceHandler *
binc_service_handler_manager_get(ServiceHandlerManager *serviceHandlerManager, const char *service_uuid);

#endif //TEST_SERVICE_HANDLER_MANAGER_H
