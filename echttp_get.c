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
 * A minimal JSON interpreter.
 *
 * This JSON interpreter was designed to test echttp_json.c, but is
 * also useful to visually inspect raw JSON data.
 *
 * SYNOPSYS:
 *
 *   echttp_get [-d] <file> element ..
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "echttp.h"
#include "echttp_json.h"
#include "echttp_xml.h"

#define PRINT_MAX 20480

static void print_string (const char *key, const char *value) {
    static char escapelist [128];

    char *buffer = (char *)malloc (3*strlen(value) + 1); // Worst case
    char *to = buffer;

    if (escapelist['n'] == 0) {
        escapelist['\"'] = '\"';
        escapelist['\\'] = '\\';
        escapelist['/'] = '/';
        escapelist[8] = 'b';
        escapelist[12] = 'f';
        escapelist[10] = 'n';
        escapelist[13] = 'r';
        escapelist[9] = 't';
    }

    while (*value) {
        char escape = escapelist[*value];
        if (escape) {
            *to++ = '\\';
            *to++ = escape;
            value++;
        } else {
            *to++ = *value++;
        }
        // TBD: Unicode?
    }
    *to = 0;

    printf ("\"%s\"\n", buffer);
    free(buffer);
}

static void print_json (ParserToken *token, int i, int deep);

static void enumerate (ParserToken *parent) {

    int i;
    int index[PRINT_MAX];
    const char *error = echttp_json_enumerate (parent, index);
    if (error) {
        printf ("error: %s\n", error);
        return;
    }

    for (i = 0; i < parent->length; ++i) {
        if (parent->type == PARSER_OBJECT)
            printf ("    %s: ", parent[index[i]].key);
        else
            printf ("    [%2d] ", i);
        print_json (parent, index[i], 0);
    }
}

static void print_json (ParserToken *token, int i, int deep) {

    switch (token[i].type) {
        case PARSER_NULL:
            printf ("null\n"); break;
        case PARSER_BOOL:
            printf ("%s\n", token[i].value.bool?"true":"false"); break;
        case PARSER_INTEGER:
            printf ("%ld\n", token[i].value.integer); break;
        case PARSER_REAL:
            printf ("%e\n", token[i].value.real); break;
        case PARSER_STRING:
            print_string (token[i].key, token[i].value.string); break;
        case PARSER_ARRAY:
            printf ("array, length %d\n", token[i].length);
            if (deep) enumerate (token+i);
            break;
        case PARSER_OBJECT:
            printf ("object, %d elements\n", token[i].length);
            if (deep) enumerate (token+i);
            break;
        default:
            fprintf (stderr, "Invalid token type %d at index %d\n",
                     token[i].type, i);
    }
}

static void print_tokens (ParserToken *token, int count) {
    int i;
    for (i = 0; i < count; ++i) {
        printf ("Token type %d at index %d, length %d, key %s\n",
                token[i].type, i, token[i].length, token[i].key?token[i].key:"(none)");
    }
}

int main (int argc, const char **argv) {

    int i;
    int count = 0;
    int index = 0;
    int show_tokens = 0;
    int xml_input = 0;
    char *buffer;
    const char *error;
    ParserToken token[PRINT_MAX];

    for (i = 1; i < argc; ++i) {
        if (strcmp (argv[i], "-d") == 0) {
            echttp_json_enable_debug();
            continue;
        }
        if (strcmp (argv[i], "-t") == 0) {
            show_tokens = 1;
            continue;
        }
        if (strcmp (argv[i], "-x") == 0) {
            xml_input = 1;
            continue;
        }

        if (!count) { // The first non-option is the file name.

            if (strstr(argv[i], ".xml")) xml_input = 1;

            buffer = echttp_parser_load (argv[i]);

            count = PRINT_MAX;
            if (xml_input)
                error = echttp_xml_parse (buffer, token, &count);
            else
                error = echttp_json_parse (buffer, token, &count);
            if (error) {
                fprintf (stderr, "Cannot decode %s: %s\n", argv[i], error);
                return -1;
            }
            if (show_tokens) print_tokens (token, count);
            continue;
        }

        index = echttp_json_search (token, argv[i]);
        printf ("%s (%d): ", argv[i], index);
        if (index >= 0) print_json (token, index, 1);
        else printf ("invalid path\n");
    }
    echttp_parser_free (buffer);
}

