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
 * int echttp_static_route (const char *uri, const char *path);
 *
 *    Declare a mapping between an URI and a local file or folder.
 *
 * void echttp_static_content_map (const char *extension, const char *content);
 *
 *    Declare an additional file content type. The most common file types are
 *    already declared, so this function should only be used in rare cases.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_static.h"
#include "echttp_catalog.h"

static echttp_catalog echttp_static_roots;

static echttp_catalog echttp_static_type;

/* Define default content type for the most frequent file extensions.
 * Don't define too many: it would load the catalog with lot of unused items.
 */
static struct {
    const char *extension;
    const char *content;
} echttp_static_default_types[] = {
    {"html", "text/html"},
    {"htm",  "text/html"},
    {"css",  "text/css"},
    {"csv",  "text/csv"},
    {"json", "application/json"},
    {"jsn",  "application/json"},
    {"js",   "application/javascript"},
    {"xml",  "text/xml"},
    {"txt",  "text/plain"},
    {"jpeg", "image/jpeg"},
    {"png",  "image/png"},
    {"gif",  "image/gif"},
    {"svg",  "image/svg+xml"},
    {0, 0}
};

static const char *echttp_static_file (int page, const char *filename) {

    char index[1024];

    if (page < 0) {
        echttp_error (404, "Not found");
        return "";
    }
    struct stat fileinfo;
    if (fstat(page, &fileinfo) < 0) goto unsupported;

    switch (fileinfo.st_mode & S_IFMT) {
    case S_IFDIR: // Use "index.html" as the default page name.
        close(page);
        snprintf (index, sizeof(index), "%s/index.html", filename);
        if (echttp_isdebug()) printf ("Directory, defaulting to %s\n", index);
        filename = index;
        page = open (filename, O_RDONLY);
        if (page < 0) goto unsupported; // Not 404: the directory exists.
        if (fstat(page, &fileinfo) < 0) goto unsupported;
        if ((fileinfo.st_mode & S_IFMT) != S_IFREG) goto unsupported;
        break;
    case S_IFREG: break; // Normal case.
    default: goto unsupported;
    }
    if (fileinfo.st_size < 0) goto unsupported;

    if (echttp_isdebug()) printf ("Serving static file: %s\n", filename);

    const char *sep = strrchr (filename, '.');
    if (sep) {
        const char *content = echttp_catalog_get (&echttp_static_type, sep+1);
        if (content) {
            echttp_content_type_set (content);
        }
    }
    echttp_transfer (page, fileinfo.st_size);
    return "";

    unsupported:
        if (echttp_isdebug()) printf ("File type violation: %s\n", filename);
        echttp_error (406, "File Not Acceptable");
        return "";
}

static const char *echttp_static_page (const char *action,
                                       const char *uri,
                                       const char *data, int length) {
    const char *path;
    char rooturi[1024];
    char filename[1024];

    if (strstr(uri, "../")) {
        if (echttp_isdebug()) printf ("Security violation: %s\n", uri);
        echttp_error (406, "Path Not Acceptable");
        return "";
    }

    strncpy (rooturi, uri, sizeof(rooturi)); // Make a writable copy.
    rooturi[sizeof(rooturi)-1] = 0;

    for(;;) {
        if (echttp_isdebug()) printf ("Searching static map for %s\n", rooturi);
        path = echttp_catalog_get (&echttp_static_roots, rooturi);
        if (path) break;
        char *sep = strrchr (rooturi+1, '/');
        if (sep == 0) break;
        *sep = 0;
    }
    if (path == 0) {
        rooturi[0] = 0;
        path = echttp_catalog_get (&echttp_static_roots, "/");
        if (path == 0) {
            echttp_error (404, "Page was lost.."); // Should never happen, but.
            return "";
        }
    }
    if (echttp_isdebug()) printf ("found match for %s: %s\n", rooturi, path);

    size_t pathlen = strlen(path);
    strncpy (filename, path, sizeof(filename));
    strncpy (filename+pathlen, uri+strlen(rooturi), sizeof(filename)-pathlen);
    filename[sizeof(filename)-1] = 0;

    return echttp_static_file (open (filename, O_RDONLY), filename);
}

static void echttp_static_initialize (void) {

    static int Initialized = 0;
    if (!Initialized) {
        // Create some common default content types.
        int i;
        for (i = 0; echttp_static_default_types[i].extension; ++i) {
            echttp_catalog_set (&echttp_static_type,
                                echttp_static_default_types[i].extension,
                                echttp_static_default_types[i].content);
        }
        Initialized = 1;
    }
}

void echttp_static_content_map (const char *extension, const char *content) {
    echttp_static_initialize();
    echttp_catalog_set (&echttp_static_type, extension, content);
}

int echttp_static_route (const char *uri, const char *path) {
    echttp_static_initialize();
    echttp_catalog_set (&echttp_static_roots, uri, path);
    return echttp_route_match (uri, echttp_static_page);
}

