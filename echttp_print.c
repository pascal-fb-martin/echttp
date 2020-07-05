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

static char *inbuffer = 0;
static char *outbuffer = 0;
static int buffer_size = 0;

#define JSON_PRINT_MAX 20480


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
    int pretty = JSON_OPTION_PRETTY;
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
        if (strcmp (argv[i], "-r") == 0) {
            pretty = 0;
            continue;
        }
        if (stat (argv[i], &filestat)) {
            fprintf (stderr, "Cannot access %s\n", argv[i]);
            continue;
        }
        if (filestat.st_size > 0) {
            if (filestat.st_size > buffer_size) {
                buffer_size = filestat.st_size + 1;
                inbuffer = (char *) realloc (inbuffer, buffer_size);
                outbuffer = (char *) realloc (outbuffer, 3*buffer_size);
            }
            fd = open (argv[i], O_RDONLY);
            if (fd < 0) {
                fprintf (stderr, "Cannot open %s\n", argv[i]);
                continue;
            }
            if (read (fd, inbuffer, filestat.st_size) != filestat.st_size) {
                fprintf (stderr, "Cannot read %s\n", argv[i]);
                continue;
            }
            close(fd);
            inbuffer[filestat.st_size] = 0; // Terminate the JSON string.

            count = JSON_PRINT_MAX;
            error = echttp_json_parse (inbuffer, token, &count);
            if (error) {
                fprintf (stderr, "Cannot decode %s: %s\n", argv[i], error);
                continue;
            }
            if (show_tokens) print_tokens (token, count);
            printf ("// File %s\n", argv[i]);
            error = echttp_json_generate (token, count, outbuffer, 3*buffer_size, pretty);
            if (error) {
                fprintf (stderr, "Cannot format: %s: %s\n", argv[i], error);
                continue;
            }
            printf ("%s", outbuffer);
        }
    }
}

