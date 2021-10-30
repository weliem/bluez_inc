//
// Created by martijn on 26/8/21.
//

#include "uuid.h"
#include <glib.h>

uuid *uuid_create(char *uuid_string) {
    g_assert(uuid_string != NULL);

    gchar* lowercase_uuid = g_ascii_strdown(uuid_string, -1);
    g_assert(g_uuid_string_is_valid(lowercase_uuid));

    uuid *result = g_new0(uuid, 1);
    strncpy((char *) result, lowercase_uuid, 36);
    g_free(lowercase_uuid);
    return result;
}
