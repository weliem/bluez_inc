//
// Created by martijn on 11-02-22.
//

#ifndef BINC_APPLICATION_H
#define BINC_APPLICATION_H

#include <gio/gio.h>
#include "forward_decl.h"

typedef GByteArray (*onLocalCharacteristicRead)(const char *path, const char* service_uuid, const char* char_uuid);
typedef void (*onLocalCharacteristicWrite)(const char *path, const char* service_uuid,
        const char* char_uuid, GByteArray *byteArray);

Application* binc_create_application();
void binc_application_free(Application *application);
const char* binc_application_get_path(Application *application);
void binc_application_publish(Application *application, Adapter *adapter);
void binc_application_add_service(Application *application, const char* service_uuid);
void binc_application_add_characteristic(Application *application, const char* service_uuid,
                                         const char* characteristic_uuid, guint8 permissions);

void binc_application_set_on_characteristic_read_callback(Application *application, onLocalCharacteristicRead callback);
void binc_application_set_on_characteristic_write_callback(Application *application, onLocalCharacteristicWrite callback);

#endif //BINC_APPLICATION_H
