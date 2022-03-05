//
// Created by martijn on 11-02-22.
//

#ifndef BINC_APPLICATION_H
#define BINC_APPLICATION_H

#include <gio/gio.h>
#include "forward_decl.h"

// Callback definitions
typedef void (*onLocalCharacteristicRead)(const Application *application, const char *address, const char *service_uuid,
                                          const char *char_uuid);

typedef void (*onLocalCharacteristicWrite)(const char *address, const char *service_uuid,
                                           const char *char_uuid, GByteArray *byteArray);

// Methods
Application *binc_create_application(const Adapter *adapter);

void binc_application_free(Application *application);

const char *binc_application_get_path(Application *application);

void binc_application_add_service(Application *application, const char *service_uuid);

void binc_application_add_characteristic(Application *application, const char *service_uuid,
                                         const char *char_uuid, guint8 permissions);

void binc_application_set_on_char_read_cb(Application *application, onLocalCharacteristicRead callback);

void binc_application_set_char_write_cb(Application *application, onLocalCharacteristicWrite callback);

void binc_application_char_set_value(const Application *application, const char *service_uuid,
                                     const char *char_uuid, GByteArray *byteArray);

void binc_application_notify(const Application *application, const char *service_uuid, const char *char_uuid,
                             GByteArray *byteArray);

#endif //BINC_APPLICATION_H
