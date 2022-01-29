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

void binc_advertisement_set_include_tx_power(Advertisement *advertisement, gboolean include_tx_power);

const char *binc_advertisement_get_path(Advertisement *advertisement);

void binc_advertisement_register(Advertisement *advertisement, Adapter *adapter);

#endif //BINC_ADVERTISEMENT_H
