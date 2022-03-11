//
// Created by martijn on 11-02-22.
//

#ifndef BINC_APPLICATION_H
#define BINC_APPLICATION_H

#include <gio/gio.h>
#include "forward_decl.h"

// Errors
#define BLUEZ_ERROR_REJECTED "org.bluez.Error.Rejected"
#define BLUEZ_ERROR_FAILED "org.bluez.Error.Failed"
#define BLUEZ_ERROR_INPROGRESS "org.bluez.Error.InProgress"
#define BLUEZ_ERROR_NOT_PERMITTED "org.bluez.Error.NotPermitted"
#define BLUEZ_ERROR_INVALID_VALUE_LENGTH "org.bluez.Error.InvalidValueLength"
#define BLUEZ_ERROR_NOT_AUTHORIZED "org.bluez.Error.NotAuthorized"
#define BLUEZ_ERROR_NOT_SUPPORTED "org.bluez.Error.NotSupported"

// This callback is called just before the characteristic's value is returned.
// Use it to update the characteristic before it is read
typedef void (*onLocalCharacteristicRead)(const Application *application, const char *address, const char *service_uuid,
                                          const char *char_uuid);

// This callback is called just before the characteristic's value is set.
// Use it to accept (return NULL), or reject (return BLUEZ_ERROR_*) the byte array
typedef char* (*onLocalCharacteristicWrite)(const Application *application, const char *address, const char *service_uuid,
                                           const char *char_uuid, GByteArray *byteArray);

// This callback is called after a characteristic's value is set, e.g. because of a 'write' or 'notify'
// Use it to act upon the new value set
typedef void (*onLocalCharacteristicUpdated)(const Application *application, const char *service_uuid,
                                            const char *char_uuid, GByteArray *byteArray);

// Methods
Application *binc_create_application(const Adapter *adapter);

void binc_application_free(Application *application);

const char *binc_application_get_path(Application *application);

void binc_application_add_service(Application *application, const char *service_uuid);

void binc_application_add_characteristic(Application *application, const char *service_uuid,
                                         const char *char_uuid, guint8 permissions);

void binc_application_set_char_read_cb(Application *application, onLocalCharacteristicRead callback);

void binc_application_set_char_write_cb(Application *application, onLocalCharacteristicWrite callback);

void binc_application_char_set_value(const Application *application, const char *service_uuid,
                                     const char *char_uuid, GByteArray *byteArray);

void binc_application_notify(const Application *application, const char *service_uuid, const char *char_uuid,
                             GByteArray *byteArray);

#endif //BINC_APPLICATION_H
