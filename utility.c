//
// Created by martijn on 6/9/21.
//

#include "utility.h"

void btox(char *dest, const guint8 *src, int n)
{
    const char xx[]= "0123456789abcdef";
    while (--n >= 0) dest[n] = xx[(src[n >> 1] >> ((1 - (n & 1)) << 2)) & 0xF];
}

GString* g_byte_array_as_hex(GByteArray *byteArray) {
    int hexLength = (int) byteArray->len * 2;
    GString *result = g_string_sized_new(hexLength + 1);
    btox(result->str, byteArray->data, hexLength);
    result->str[hexLength] = 0;
    result->len = hexLength;
    return result;
}

GList* g_variant_string_array_to_list(GVariant *value) {
    g_assert(value != NULL);

    const gchar *type = g_variant_get_type_string(value);
    if (g_strcmp0(type, "as")) return NULL;

    GList *list = NULL;
    gchar *data;
    GVariantIter iter;

    g_variant_iter_init(&iter, value);
    while (g_variant_iter_next(&iter, "s", &data)) {
        list = g_list_append(list, data);
    }
    return list;
}

