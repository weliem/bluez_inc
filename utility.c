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

#include "utility.h"
#include "math.h"

void bytes_to_hex(char *dest, const guint8 *src, int n) {
    const char xx[] = "0123456789abcdef";
    while (--n >= 0) dest[n] = xx[(src[n >> 1] >> ((1 - (n & 1)) << 2)) & 0xF];
}

GString *g_byte_array_as_hex(const GByteArray *byteArray) {
    int hexLength = (int) byteArray->len * 2;
    GString *result = g_string_sized_new(hexLength + 1);
    bytes_to_hex(result->str, byteArray->data, hexLength);
    result->str[hexLength] = 0;
    result->len = hexLength;
    return result;
}

GList *g_variant_string_array_to_list(GVariant *value) {
    g_assert(value != NULL);
    g_assert(g_str_equal(g_variant_get_type_string(value), "as"));

    GList *list = NULL;
    gchar *data;
    GVariantIter iter;

    g_variant_iter_init(&iter, value);
    while (g_variant_iter_loop(&iter, "s", &data)) {
        list = g_list_append(list, g_strdup(data));
    }
    return list;
}

float binc_round_with_precision(float value, guint8 precision) {
    int multiplier = (int) pow(10.0, precision);
    return roundf(value * multiplier) / multiplier;
}

gchar *binc_date_time_format_iso8601(GDateTime *datetime) {
    GString *outstr = NULL;
    gchar *main_date = NULL;
    gint64 offset;
    gchar *format = "%Y-%m-%dT%H:%M:%S";

    /* Main date and time. */
    main_date = g_date_time_format(datetime, format);
    outstr = g_string_new(main_date);
    g_free(main_date);

    /* Timezone. Format it as `%:::z` unless the offset is zero, in which case
     * we can simply use `Z`. */
    offset = g_date_time_get_utc_offset(datetime);

    if (offset == 0) {
        g_string_append_c (outstr, 'Z');
    } else {
        gchar *time_zone = g_date_time_format(datetime, "%:z");
        g_string_append(outstr, time_zone);
        g_free(time_zone);
    }

    return g_string_free(outstr, FALSE);
}

