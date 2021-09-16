//
// Created by martijn on 14/9/21.
//

#include "parser.h"
#include "math.h"

// IEEE 11073 Reserved float values
typedef enum {
    MDER_POSITIVE_INFINITY = 0x007FFFFE,
    MDER_NaN = 0x007FFFFF,
    MDER_NRes = 0x00800000,
    MDER_RESERVED_VALUE = 0x00800001,
    MDER_NEGATIVE_INFINITY = 0x00800002
} ReservedFloatValues;

static const double reserved_float_values[5] = {MDER_POSITIVE_INFINITY, MDER_NaN, MDER_NaN, MDER_NaN, MDER_NEGATIVE_INFINITY};

Parser* parser_create(GByteArray *bytes, int byteOrder) {
    Parser* parser = g_new0(Parser, 1);
    parser->bytes = bytes;
    parser->offset = 0;
    parser->byteOrder = byteOrder;
}

guint16 parser_get_uint16(Parser *parser) {
    g_assert((parser->offset+1) < parser->bytes->len);

    guint8 byte1, byte2;
    byte1 = parser->bytes->data[parser->offset];
    byte2 = parser->bytes->data[parser->offset+1];
    parser->offset = parser->offset + 2;
    if (parser->byteOrder == LITTLE_ENDIAN) {
        return (byte2 << 8) + byte1;
    } else {
        return (byte1 << 8) + byte2;
    }
}

guint32 parser_get_uint32(Parser *parser) {
    g_assert((parser->offset + 3) < parser->bytes->len);

    guint8 byte1, byte2, byte3, byte4;
    byte1 = parser->bytes->data[parser->offset];
    byte2 = parser->bytes->data[parser->offset+1];
    byte3 = parser->bytes->data[parser->offset+2];
    byte4 = parser->bytes->data[parser->offset+3];
    parser->offset = parser->offset + 4;
    if (parser->byteOrder == LITTLE_ENDIAN) {
        return (byte4 << 24) + (byte3 << 16) + (byte2 << 8) + byte1;
    } else {
        return (byte1 << 24) + (byte2 << 16) + (byte3 << 8) + byte4;
    }
}

float parser_get_sfloat(Parser *parser) {
    g_assert(parser->offset < parser->bytes->len);

    guint16 sfloat = parser_get_uint16(parser);

    int mantissa = sfloat & 0xfff;
    if (mantissa >= 0x800) {
        mantissa = mantissa - 0x1000;
    }
    int exponent = sfloat >> 12;
    if (exponent >= 0x8) {
        exponent = exponent - 0x10;
    }
    return (float) (mantissa * pow(10.0, exponent));
}

/* round number n to d decimal points */
float fround(float n, int d)
{
    int rounded = floor(n * pow(10.0f, d) + 0.5f);
    int divider = (int) pow(10.0f, d);
    return  (float) rounded/ (float) divider;
}

float parser_get_float(Parser *parser)
{
    guint32 int_data = parser_get_uint32(parser);

    guint32 mantissa = int_data & 0xFFFFFF;
    gint8 exponent = int_data >> 24;
    double output = 0;

    if (mantissa >= MDER_POSITIVE_INFINITY &&
        mantissa <= MDER_NEGATIVE_INFINITY) {
        output = reserved_float_values[mantissa - MDER_POSITIVE_INFINITY];
    } else {
        if (mantissa >= 0x800000) {
            mantissa = -((0xFFFFFF + 1) - mantissa);
        }
        output = (mantissa * pow(10.0f, exponent));
    }

    return (float) output;
}