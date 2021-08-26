//
// Created by martijn on 26/8/21.
//

#include "uuid.h"
#include <glib.h>

uuid *uuid_create(char *uuid_string) {
    if (uuid_string == NULL) return NULL;

    GString *original_uuid = g_string_new(uuid_string);
    GString *lowercase_uuid = g_string_ascii_down(original_uuid);

    if (g_uuid_string_is_valid(lowercase_uuid->str)) {
        uuid *result = g_malloc(sizeof(uuid));
        strncpy((char *) result, lowercase_uuid->str, 36);
        return result;
    } else return NULL;
}
