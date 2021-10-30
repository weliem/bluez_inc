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
