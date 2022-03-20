//
// Created by martijn on 20-03-22.
//

#ifndef BINC_DESCRIPTOR_INTERNAL_H
#define BINC_DESCRIPTOR_INTERNAL_H

#include "descriptor.h"

Descriptor *binc_descriptor_create(Device *device, const char *path);

void binc_descriptor_free(Descriptor *descriptor);

void binc_descriptor_set_read_cb(Descriptor *descriptor, OnDescReadCallback callback);

void binc_descriptor_set_write_cb(Descriptor *descriptor, OnDescWriteCallback callback);

void binc_descriptor_set_uuid(Descriptor *descriptor, const char *uuid);

void binc_descriptor_set_char_path(Descriptor *descriptor, const char *path);

void binc_descriptor_set_char(Descriptor *descriptor, Characteristic *characteristic);

void binc_descriptor_set_flags(Descriptor *descriptor, GList *flags);

const char *binc_descriptor_get_char_path(const Descriptor *descriptor);

#endif //BINC_DESCRIPTOR_INTERNAL_H
