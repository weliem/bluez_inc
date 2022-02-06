//
// Created by martijn on 29-01-22.
//

#ifndef BINC_ADVERTISEMENT_H
#define BINC_ADVERTISEMENT_H

#include <gio/gio.h>
#include "forward_decl.h"

Advertisement *binc_advertisement_create();

void binc_advertisement_free(Advertisement *advertisement);

void binc_advertisement_set_local_name(Advertisement *advertisement, const char* local_name);

void binc_advertisement_set_services(Advertisement *advertisement, const GPtrArray * service_uuids);

void binc_advertisement_set_manufacturer_data(Advertisement *advertisement, guint16 manufacturer_id, const GByteArray *byteArray);

void binc_advertisement_set_service_data(Advertisement *advertisement, const char* service_uuid, const GByteArray *byteArray);

const char *binc_advertisement_get_path(Advertisement *advertisement);

void binc_advertisement_register(Advertisement *advertisement, const Adapter *adapter);

#endif //BINC_ADVERTISEMENT_H
