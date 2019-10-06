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
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * void echttp_static_map (const char *uri, const char *path);
 *
 *    Declare a mapping between an URI and a local file or folder.
 *
 * const char *echttp_static_page (const char *action,
 *                                 const char *uri,
 *                                 const char *data, int length);
 *
 *    Execute an HTTP request for a static page (i.e. file).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "echttp.h"
#include "echttp_catalog.h"

static echttp_catalog echttp_static_roots;

static echttp_catalog echttp_static_type;

static char *echttp_static_buffer = 0;
static int echttp_static_buffer_size = 0;

static void echttp_static_initialize (void) {

    static int Initialized = 0;
    if (!Initialized) {
        // Create some common default content types.
        echttp_catalog_set (&echttp_static_type, "html", "text/html");
        echttp_catalog_set (&echttp_static_type, "htm",  "text/html");
        echttp_catalog_set (&echttp_static_type, "json", "application/json");
        echttp_catalog_set (&echttp_static_type, "jsn",  "application/json");
        echttp_catalog_set (&echttp_static_type, "css",  "text/css");
        Initialized = 1;
    }
}

void echttp_static_content_map (const char *extension, const char *content) {
    echttp_static_initialize();
    echttp_catalog_set (&echttp_static_type, extension, content);
}

void echttp_static_map (const char *uri, const char *path) {
    echttp_static_initialize();
    echttp_catalog_set (&echttp_static_roots, uri, path);
}

const char *echttp_static_page (const char *action,
                                const char *uri,
                                const char *data, int length) {
    const char *path;
    char rooturi[1024];
    char filename[1024];
    char *sep;

    strncpy (rooturi, uri, sizeof(rooturi)); // Make a writable copy.
    rooturi[sizeof(rooturi)-1] = 0;

    for(;;) {
        path = echttp_catalog_get (&echttp_static_roots, rooturi);
        if (path) break;
        sep = strrchr (rooturi, '/');
        if ((sep == 0) || (sep == path)) {
            echttp_error (404, "Page was lost.."); // Should never happen, but.
            return "";
        }
        *sep = 0;
    }
    strncpy (filename, path, sizeof(filename));
    strncpy (filename+strlen(path),
             uri+strlen(rooturi), sizeof(filename)-strlen(path));
    filename[sizeof(filename)-1] = 0;

    FILE *page = fopen (filename, "r");
    if (page == 0) {
        echttp_error (404, "Not found");
        return "";
    }

    fseek (page, 0, SEEK_END);
    size_t size = ftell (page) + 1;
    rewind(page);
    if (size > echttp_static_buffer_size) {
        echttp_static_buffer_size = size;
        echttp_static_buffer = realloc (echttp_static_buffer, size);
    }
    echttp_static_buffer[0] = 0;
    fread (echttp_static_buffer, 1, size, page);
    echttp_static_buffer[size-1] = 0;
    fclose (page);

    sep = strrchr (filename, '.');
    if (sep) {
        const char *content = echttp_catalog_get (&echttp_static_type, sep+1);
        if (content) {
            echttp_content_type_set (content);
        }
    }
    return echttp_static_buffer;
}

