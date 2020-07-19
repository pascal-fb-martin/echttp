/* echttp - Embedded HTTP server.
 *
 * Copyright 2020, Pascal Martin
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
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * This module provides functions meant to make parsing easier.
 *
 * char *echttp_parser_load (const char *file);
 *
 *    Return the whole file content as one string. The buffer is dynamically
 *    allocated and must be released using echttp_parser_free().
 *
 *    Returns 0 if the file does not exist or cannot be read. In this case
 *    check the value of errno. If errno indicates no error, the file was
 *    empty.
 *
 * void echttp_parser_free (char *buffer);
 *
 *    Free the provided buffer (must have been allocated in this module).
 */

#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "echttp.h"
#include "echttp_parser.h"

// These are used for consistency checks to catch common mistakes, such as
// freeing a static buffer.
//
static char *BoundLow = 0;
static char *BoundHigh = 0;

char *echttp_parser_load (const char *file) {

    struct stat filestat;
    int fd;
    int i;
    char *buffer;

    fd = open (file, O_RDONLY);
    if (fd < 0) return 0;

    if (fstat (fd, &filestat)) return 0;
    if (filestat.st_size <= 0) return 0;

    buffer = (char *) malloc(filestat.st_size+4);
    if (read (fd, buffer, filestat.st_size) != filestat.st_size) return 0;
    buffer[filestat.st_size] = 0;
    close (fd);

    if (!BoundLow || buffer < BoundLow) BoundLow = buffer;
    if (!BoundHigh || buffer > BoundHigh) BoundHigh = buffer;

    return buffer;
}

void echttp_parser_free (char *buffer) {

    // Do not free an address that we cannot have allocated before.
    //
    if (buffer < BoundHigh && buffer > BoundLow) {
        free (buffer);
    }
}

