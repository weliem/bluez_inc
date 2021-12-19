/*
 *   Copyright (c) 2021 Martijn van Welie
 *
 *   Permission is hereby granted, free of charge, to any person obtaining a copy
 *   of this software and associated documentation files (the "Software"), to deal
 *   in the Software without restriction, including without limitation the rights
 *   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *   copies of the Software, and to permit persons to whom the Software is
 *   furnished to do so, subject to the following conditions:
 *
 *   The above copyright notice and this permission notice shall be included in all
 *   copies or substantial portions of the Software.
 *
 *   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *   LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *   OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *   SOFTWARE.
 *
 */

#ifndef TEST_CHARACTERISTIC_H
#define TEST_CHARACTERISTIC_H

#include <gio/gio.h>
#include "service.h"
#include "forward_decl.h"

/*
 * GATT Characteristic Property bit field
 * Reference: Core SPEC 4.1 page 2183 (Table 3.5: Characteristic Properties
 * bit field) defines how the Characteristic Value can be used, or how the
 * characteristic descriptors (see Section 3.3.3 - page 2184) can be accessed.
 * In the core spec, regular properties are included in the characteristic
 * declaration, and the extended properties are defined as descriptor.
 */
#define GATT_CHR_PROP_BROADCAST                0x01
#define GATT_CHR_PROP_READ                    0x02
#define GATT_CHR_PROP_WRITE_WITHOUT_RESP    0x04
#define GATT_CHR_PROP_WRITE                    0x08
#define GATT_CHR_PROP_NOTIFY                0x10
#define GATT_CHR_PROP_INDICATE                0x20
#define GATT_CHR_PROP_AUTH                    0x40
#define GATT_CHR_PROP_EXT_PROP                0x80


typedef enum WriteType {
    WITH_RESPONSE = 0, WITHOUT_RESPONSE = 1
} WriteType;

typedef void (*OnNotifyingStateChangedCallback)(Characteristic *characteristic, const GError *error);

typedef void (*OnNotifyCallback)(Characteristic *characteristic, const GByteArray *byteArray);

typedef void (*OnReadCallback)(Characteristic *characteristic, const GByteArray *byteArray, const GError *error);

typedef void (*OnWriteCallback)(Characteristic *characteristic, const GError *error);


void binc_characteristic_read(Characteristic *characteristic);

void binc_characteristic_write(Characteristic *characteristic, const GByteArray *byteArray, WriteType writeType);

void binc_characteristic_start_notify(Characteristic *characteristic);

void binc_characteristic_stop_notify(Characteristic *characteristic);

Device* binc_characteristic_get_device(const Characteristic *characteristic);

const char* binc_characteristic_get_uuid(const Characteristic *characteristic);

const char* binc_characteristic_get_service_uuid(const Characteristic *characteristic);

const char* binc_characteristic_get_service_path(const Characteristic *characteristic);

GList* binc_characteristic_get_flags(const Characteristic *characteristic);

guint binc_characteristic_get_properties(const Characteristic *characteristic);

gboolean binc_characteristic_is_notifying(const Characteristic *characteristic);

gboolean binc_characteristic_supports_write(const Characteristic *characteristic, WriteType writeType);

gboolean binc_characteristic_supports_read(const Characteristic *characteristic);

gboolean binc_characteristic_supports_notify(const Characteristic *characteristic);


/**
 * Get a string representation of the characteristic
 * @param characteristic
 * @return string representation of characteristic, caller must free
 */
char *binc_characteristic_to_string(const Characteristic *characteristic);

#endif //TEST_CHARACTERISTIC_H
