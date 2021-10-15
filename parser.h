//
// Created by martijn on 14/9/21.
//

#ifndef TEST_PARSER_H
#define TEST_PARSER_H

#include <glib.h>
#include <endian.h>

typedef struct parser_instance Parser;

/**
 * Create a parser for a byte array
 *
 * @param bytes the byte array
 * @param byteOrder either LITTLE_ENDIAN or BIG_ENDIAN
 * @return parser object
 */
Parser *parser_create(GByteArray *bytes, int byteOrder);

void parser_set_offset(Parser *parser, guint offset);

void parser_free(Parser *parser);

guint8 parser_get_uint8(Parser *parser);

guint16 parser_get_uint16(Parser *parser);

guint32 parser_get_uint32(Parser *parser);

float parser_get_sfloat(Parser *parser);

float parser_get_float(Parser *parser);

GByteArray *binc_get_current_time();

GString *parser_get_string(Parser *parser);

#endif //TEST_PARSER_H
