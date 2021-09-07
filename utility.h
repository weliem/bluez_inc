//
// Created by martijn on 6/9/21.
//

#ifndef TEST_UTILITY_H
#define TEST_UTILITY_H

#include <glib.h>

GString* g_byte_array_as_hex(GByteArray *byteArray);
GList* g_variant_string_array_to_list(GVariant *value);

#endif //TEST_UTILITY_H
