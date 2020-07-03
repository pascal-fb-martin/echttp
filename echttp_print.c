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

static const char *indentation(int depth) {
    static char indent[81];
    if (indent[0] == 0) memset (indent, ' ',sizeof(indent)-1);
    return indent + sizeof(indent) -1 - (4 * depth);
}

static void print_null (int depth, const char *key, int comma) {
    if (key)
        printf ("%s\"%s\" : null%s\n", indentation(depth),
                key, comma?",":"");
    else
        printf ("%snull%s\n", indentation(depth), comma?",":"");
}

static void print_bool (int depth, const char *key, int value, int comma) {
    if (key)
        printf ("%s\"%s\" : %s%s\n", indentation(depth),
                key, value?"true":"false", comma?",":"");
    else
        printf ("%s%s%s\n", indentation(depth),
                value?"true":"false", comma?",":"");
}

static void print_integer (int depth, const char *key, long value, int comma) {
    if (key)
        printf ("%s\"%s\" : %d%s\n", indentation(depth),
                key, value, comma?",":"");
    else
        printf ("%s%d%s\n", indentation(depth),
                value, comma?",":"");
}

static void print_real (int depth, const char *key, double value, int comma) {
    if (key)
        printf ("%s\"%s\" : %e%s\n", indentation(depth),
                key, value, comma?",":"");
    else
        printf ("%s%e%s\n", indentation(depth),
                value, comma?",":"");
}

static void print_string (int depth, const char *key, const char *value, int comma) {
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

    if (key)
        printf ("%s\"%s\" : \"%s\"%s\n", indentation(depth),
                key, buffer, comma?",":"");
    else
        printf ("%s\"%s\"%s\n", indentation(depth),
                buffer, comma?",":"");
    free(buffer);
}

static void print_compound (int depth, const char *key, char value, int comma) {
    if (key)
        printf ("%s\"%s\" : %c%s\n", indentation(depth),
                key, value, comma?",":"");
    else
        printf ("%s%c%s\n", indentation(depth),
                value, comma?",":"");
}

static void print_json (const char *name, JsonToken *token, int count) {

    int i;
    int depth = 0;
    int ending[32];
    int countdown[32];

    countdown[0] = 0;

    printf ("// File %s\n", name);

    for (i = 0; i < count; ++i) {

        switch (token[i].type) {
            case JSON_NULL:
                print_null (depth, token[i].key, countdown[depth]>1); break;
            case JSON_BOOL:
                print_bool (depth, token[i].key, token[i].value.bool, countdown[depth]>1); break;
            case JSON_INTEGER:
                print_integer (depth, token[i].key, token[i].value.integer, countdown[depth]>1); break;
            case JSON_REAL:
                print_real (depth, token[i].key, token[i].value.real, countdown[depth]>1); break;
            case JSON_STRING:
                print_string (depth, token[i].key, token[i].value.string, countdown[depth]>1); break;
            case JSON_ARRAY:
                print_compound (depth, token[i].key, '[', 0);
                depth += 1;
                ending[depth] = ']';
                countdown[depth] = token[i].length + 1;
                break;
            case JSON_OBJECT:
                print_compound (depth, token[i].key, '{', 0);
                depth += 1;
                ending[depth] = '}';
                countdown[depth] = token[i].length + 1;
                break;
            default:
                fprintf (stderr, "Invalid token type %d at index %d\n",
                         token[i].type, i);
        }

        while (depth > 0) {
            countdown[depth] -= 1;
            if (countdown[depth] > 0) break;
            depth -= 1;
            print_compound (depth, 0, ending[depth+1], countdown[depth]>1);
        }
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
    int count;
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
        if (stat (argv[i], &filestat)) {
            fprintf (stderr, "Cannot access %s\n", argv[i]);
            continue;
        }
        if (filestat.st_size > 0) {
            if (filestat.st_size > buffer_size) {
                buffer_size = filestat.st_size + 1;
                buffer = (char *) realloc (buffer, buffer_size);
            }
            fd = open (argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf (stderr, "Cannot open %s\n", argv[i]);
                continue;
            }
            if (read (fd, buffer, filestat.st_size) != filestat.st_size) {
                fprintf (stderr, "Cannot read %s\n", argv[i]);
                continue;
            }
            close(fd);
            buffer[filestat.st_size] = 0; // Terminate the JSON string.

            count = JSON_PRINT_MAX;
            error = echttp_json_parse (buffer, token, &count);
            if (error) {
                fprintf (stderr, "Cannot decode %s: %s\n", argv[i], error);
                continue;
            }
            if (show_tokens) print_tokens (token, count);
            print_json (argv[i], token, count);
        }
    }
}

