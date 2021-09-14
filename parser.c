//
// Created by martijn on 14/9/21.
//

#include "parser.h"
#include "math.h"

Parser* parser_create(GByteArray *bytes, int byteOrder) {
    Parser* parser = g_new0(Parser, 1);
    parser->bytes = bytes;
    parser->offset = 0;
    parser->byteOrder = byteOrder;
}

guint16 parser_get_uint16(Parser *parser) {
    g_assert(parser->offset < parser->bytes->len);

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