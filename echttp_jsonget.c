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
 *   echttp_jsonget [-d] <file> element ..
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

static char *buffer = 0;
static int buffer_size = 0;

#define JSON_PRINT_MAX 1024

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

static void print_json (JsonToken *token, int i, int deep);

static void enumerate (JsonToken *parent) {

    int i;
    int index[JSON_PRINT_MAX];
    const char *error = echttp_json_enumerate (parent, index);
    if (error) {
        printf ("error: %s\n", error);
        return;
    }

    for (i = 0; i < parent->length; ++i) {
        if (parent->type == JSON_OBJECT)
            printf ("    %s: ", parent[index[i]].key);
        else
            printf ("    [%2d] ", i);
        print_json (parent, index[i], 0);
    }
}

static void print_json (JsonToken *token, int i, int deep) {

    switch (token[i].type) {
        case JSON_NULL:
            printf ("null\n"); break;
        case JSON_BOOL:
            printf ("%s\n", token[i].value.bool?"true":"false"); break;
        case JSON_INTEGER:
            printf ("%d\n", token[i].value.integer); break;
        case JSON_REAL:
            printf ("%e\n", token[i].value.real); break;
        case JSON_STRING:
            print_string (token[i].key, token[i].value.string); break;
        case JSON_ARRAY:
            printf ("array, length %d\n", token[i].length);
            if (deep) enumerate (token+i);
            break;
        case JSON_OBJECT:
            printf ("object, %d elements\n", token[i].length);
            if (deep) enumerate (token+i);
            break;
        default:
            fprintf (stderr, "Invalid token type %d at index %d\n",
                     token[i].type, i);
    }
}

static void print_tokens (JsonToken *token, int count) {
    int i;
    for (i = 0; i < count; ++i) {
        printf ("Token type %d at index %d, length %d, key %s\n",
                token[i].type, i, token[i].length, token[i].key?token[i].key:"(none)");
    }
}

int main (int argc, const char **argv) {

    int i, j;
    int fd;
    int count = 0;
    int index = 0;
    int show_tokens = 0;
    const char *error;
    struct stat filestat;
    JsonToken token[JSON_PRINT_MAX];

    for (i = 1; i < argc; ++i) {
        if (strcmp (argv[i], "-d") == 0) {
            echttp_json_enable_debug();
            continue;
        }
        if (strcmp (argv[i], "-t") == 0) {
            show_tokens = 1;
            continue;
        }
        if (!count) {
            if (stat (argv[i], &filestat)) {
                fprintf (stderr, "Cannot access %s\n", argv[i]);
                return -1;
            }
            if (filestat.st_size <= 0) {
                fprintf (stderr, "%s contains no data\n", argv[i]);
                return -1;
            }
            if (filestat.st_size > buffer_size) {
                buffer = (char *) realloc (buffer, filestat.st_size);
                buffer_size = filestat.st_size;
            }
            fd = open (argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf (stderr, "Cannot open %s\n", argv[i]);
                return -1;
            }
            if (read (fd, buffer, filestat.st_size) != filestat.st_size) {
                fprintf (stderr, "Cannot read %s\n", argv[i]);
                return -1;
            }
            close(fd);
            count = JSON_PRINT_MAX;
            error = echttp_json_parse (buffer, token, &count);
            if (error) {
                fprintf (stderr, "Cannot decode %s: %s\n", argv[i], error);
                return -1;
            }
            if (show_tokens) print_tokens (token, count);
            continue;
        }
        index = echttp_json_search (token, count, argv[i]);
        printf ("%s (%d): ", argv[i], index);
        if (index >= 0) print_json (token, index, 1);
        else printf ("invalid name\n");
    }
}

