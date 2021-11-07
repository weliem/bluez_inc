//
// Created by martijn on 04-11-21.
//

#ifndef TEST_SERVICE_HANDLER_MANAGER_H
#define TEST_SERVICE_HANDLER_MANAGER_H

#include <glib.h>
#include "forward_decl.h"

typedef void (*onCharacteristicsDiscovered)(gpointer *handler, Device *device);

typedef void (*onNotificationStateUpdated)(gpointer *handler, Device *device, Characteristic *characteristic, GError *error);

typedef void (*onCharacteristicWrite)(gpointer *handler, Device *device, Characteristic *characteristic,
                                      GByteArray *byteArray, GError *error);

typedef void (*onCharacteristicChanged)(gpointer *handler, Device *device, Characteristic *characteristic,
                                        GByteArray *byteArray, GError *error);

typedef void (*serviceHandlerFree)(gpointer *handler);

struct service_handler_entry {
    gpointer handler;
    const char *uuid;
    onCharacteristicsDiscovered on_characteristics_discovered;
    onNotificationStateUpdated on_notification_state_updated;
    onCharacteristicWrite on_characteristic_write;
    onCharacteristicChanged on_characteristic_changed;
    serviceHandlerFree service_handler_free;
};

typedef struct service_handler_entry ServiceHandler;

ServiceHandlerManager *binc_service_handler_manager_create();

void binc_service_handler_manager_free(ServiceHandlerManager *serviceHandlerManager);

void binc_service_handler_manager_add(ServiceHandlerManager *serviceHandlerManager, ServiceHandler *service_handler);

ServiceHandler *
binc_service_handler_manager_get(ServiceHandlerManager *serviceHandlerManager, const char *service_uuid);

#endif //TEST_SERVICE_HANDLER_MANAGER_H
