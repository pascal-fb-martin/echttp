/* echttp - Embedded HTTP server.
 *
 * Copyright 2019, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * ------------------------------------------------------------------------
 *
 * A minimal JSON pretty printer.
 *
 * This JSON pretty printer was designed to test echttp_json.c, but is
 * also useful to visually inspect raw JSON data.
 *
 * SYNOPSYS:
 *
 *   echttp_print <file>..
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "echttp.h"
#include "echttp_json.h"
#include "echttp_xml.h"


static void print_tokens (ParserToken *token, int count) {
    int i;
    static const char *type2name[16] = {0};

    if (!type2name[PARSER_OBJECT]) {
        type2name[PARSER_NULL] = "null";
        type2name[PARSER_BOOL] = "bool";
        type2name[PARSER_INTEGER] = "integer";
        type2name[PARSER_REAL] = "real";
        type2name[PARSER_STRING] = "string";
        type2name[PARSER_ARRAY] = "array";
        type2name[PARSER_OBJECT] = "object";
    }
    for (i = 0; i < count; ++i) {
        char typeascii[10];
        char valueascii[32];
        const char *name = type2name[token[i].type];
        const char *value;
        if (!name) {
            snprintf (typeascii, sizeof(typeascii), "%d", token[i].type);
            name = typeascii;
        }
        switch (token[i].type) {
            case PARSER_NULL:
                value = "null";
                break;
            case PARSER_BOOL:
                value = (token[i].value.bool)?"true":"false";
                break;
            case PARSER_INTEGER:
                snprintf (valueascii, sizeof(valueascii), "%lld", token[i].value.integer);
                value = valueascii;
                break;
            case PARSER_REAL:
                snprintf (valueascii, sizeof(valueascii), "%e", token[i].value.real);
                value = valueascii;
                break;
            case PARSER_STRING:
                value = token[i].value.string;
                break;
            default: value = "";
        }
        printf ("Token type %s at index %d, length %d, key %s, value = %s\n",
                name, i, token[i].length, token[i].key?token[i].key:"(none)", value);
    }
}

int main (int argc, const char **argv) {

    int i;
    int size;
    int count;
    int show_tokens = 0;
    int force_xml = 0; // default is JSON.
    int xml_input;
    int pretty = PRINT_OPTION_PRETTY;
    int silent = 0;
    const char *error;
    ParserToken *token = 0;
    int max = 0;
    int estimated;

    char *inbuffer = 0;
    char *outbuffer = 0;
    int outbuffer_size = 0;

    for (i = 1; i < argc; ++i) {
        if (strcmp (argv[i], "-d") == 0) {
            echttp_json_enable_debug();
            echttp_xml_enable_debug();
            continue;
        }
        if (strcmp (argv[i], "-t") == 0) {
            show_tokens = 1;
            continue;
        }
        if (strcmp (argv[i], "-x") == 0) {
            force_xml = 1; // Force XML no matter what.
            continue;
        }
        if (strcmp (argv[i], "-r") == 0) {
            pretty = 0;
            continue;
        }
        if (strcmp (argv[i], "-s") == 0) {
            silent = 1;
            continue;
        }
        xml_input = force_xml;
        if (strstr(argv[i], ".xml")) xml_input = 1;

        inbuffer = echttp_parser_load (argv[i]);
        if (!inbuffer) {
            fprintf (stderr, "Cannot access %s\n", argv[i]);
            continue;
        }
        size = strlen(inbuffer);
        if (outbuffer_size < 30 * size) {
            outbuffer_size = 30 * size;
            outbuffer = (char *) realloc (outbuffer, outbuffer_size);
            if (!outbuffer) {
                fprintf (stderr, "Not enough memory for a %d bytes output buffer\n", outbuffer_size);
                exit(2);
            }
        }

        if (xml_input) {
            estimated = echttp_xml_estimate (inbuffer);
            printf ("// File %s: estimated %d XML tokens\n", argv[i], estimated);
            if (estimated > max) {
                token = realloc (token, estimated * sizeof(*token));
                if (!token) {
                    fprintf (stderr, "Not enough memory for %d token\n", estimated);
                    exit(1);
                }
                max = estimated;
            }
            count = max;
            error = echttp_xml_parse (inbuffer, token, &count);
        } else {
            estimated = echttp_json_estimate (inbuffer);
            printf ("// File %s: estimated %d JSON tokens\n", argv[i], estimated);
            if (estimated > max) {
                token = realloc (token, estimated * sizeof(*token));
                if (!token) {
                    fprintf (stderr, "Not enough memory for %d token\n", estimated);
                    exit(1);
                }
                max = estimated;
            }
            count = max;
            error = echttp_json_parse (inbuffer, token, &count);
        }
        if (error) {
            fprintf (stderr,
                     "%s: error after %d tokens, %s\n", argv[i], count, error);
            continue;
        }
        if (show_tokens) print_tokens (token, count);
        printf ("// File %s (%d characters, %d tokens)\n",
                argv[i], size, count);

        if (!silent) {
            error = echttp_json_format
                        (token, count, outbuffer, outbuffer_size, pretty);
            if (error) {
                fprintf (stderr, "Cannot format: %s: %s\n", argv[i], error);
                continue;
            }
            printf ("%s", outbuffer);
        }
        echttp_parser_free (inbuffer);
    }
    // These are useless (we exit anyway), make static analysis happy.
    if (token) free(token);
    if (outbuffer) free(outbuffer);
    return 0;
}

