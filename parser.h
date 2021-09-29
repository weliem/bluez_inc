//
// Created by martijn on 14/9/21.
//

#ifndef TEST_PARSER_H
#define TEST_PARSER_H

#include <glib.h>
#include <endian.h>

typedef struct {
    GByteArray *bytes;
    int offset;
    int byteOrder;
} Parser;

Parser* parser_create(GByteArray *bytes, int byteOrder);
void parser_free(Parser *parser);
guint8 parser_get_uint8(Parser *parser);
guint16 parser_get_uint16(Parser *parser);
guint32 parser_get_uint32(Parser *parser);
float parser_get_sfloat(Parser *parser);
float parser_get_float(Parser *parser);

GByteArray* binc_get_current_time();

#endif //TEST_PARSER_H
