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
 * void echttp_static_default (const char *arg);
 *
 *    Declare a default option, which will be ignored if an alternate value
 *    is provided in the command line (see below).
 *    
 * void echttp_static_initialize (int argc, const char *argv[]);
 *
 *    Initialize this module according to command line options.
 *
 *    Compatibility with previous versions: this initialization is optional.
 *
 * int echttp_static_route (const char *uri, const char *path);
 *
 *    Declare a mapping between an URI and a local file or folder.
 *    The same URI may be mapped multiple times: only the latest path
 *    declared will be used. This allows applications to move the root
 *    directory while running.
 *
 * typedef int echttp_not_found_handler (const char *path);
 * echttp_not_found_handler *echttp_static_on_not_found (echttp_not_found_handler *handler);
 *
 *    Declare a handler that is called when the target URI is not found. This
 *    gives the application a chance to create the target file on the fly.
 *    This function returns the previous handler, allowing the application
 *    to chain handlers, if needed.
 *
 *    The handler must return a file descriptor to the file at the given path,
 *    or -1 if the file cannot be created.
 *
 * void echttp_static_content_map (const char *extension, const char *content);
 *
 *    Declare an additional file content type. The most common file types are
 *    already declared, so this function should only be used in rare cases.
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <magic.h>

#include "echttp.h"
#include "echttp_static.h"
#include "echttp_catalog.h"

static magic_t echttp_magic_cookie = 0;

static echttp_catalog echttp_static_roots;

static echttp_catalog echttp_static_type;

static int echttp_static_cannot_find (const char *path) {return -1;}
static echttp_not_found_handler *echttp_static_not_found = echttp_static_cannot_find;

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
    {"avi",  "video/x-msvideo"},
    {"mkv",  "video/x-matroska"},
    {"mp4",  "video/mp4"},
    {"mp3",  "audio/mpeg"},
    {"gz",   "application/gzip"},
    {"zip",  "application/zip"},
    {"7z",   "application/x-7z-compressed"},
    {"pdf",  "application/pdf"},
    {"svg",  "image/svg+xml"},
    {0, 0}
};

static const char *echttp_static_type_fallback (void) {

    // This function is called when the file content type cannot be determined.
    // This function always returns something..
    // The default behavior is to describe the data as a sequence of bytes.
    // We cheat a little: if the client does not accept any application
    // type but accept text types, then pretend this is plain text..
    // (Note that both values are string litterals, i.e. inherently static.)
    //
    const char *accepted = echttp_attribute_get ("Accept");
    if (accepted) {
        if (!strstr (accepted, "application/")) {
            if (strstr (accepted, "text/")) return "text/plain";
        }
    }
    return "application/octet-stream";
}

static const char *echttp_static_file (int page, const char *filename) {

    char index[1040];

    // Give the application a last chance to create the page on the fly.
    if (page < 0) page = echttp_static_not_found (filename);

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

    const char *condition = echttp_attribute_get ("If-Modified-Since");
    if (condition) {
        // The client wants to know if the file has changed; if not,
        // send HTTP status 302 and stop there.
        // Example of time format: Thu, 31 Jul 2025 02:05:19 GMT
        struct tm time_info = {0};
        const char *format = "%a, %d %b %Y %H:%M:%S";
        char *result = strptime (condition, format, &time_info);
        if (result) {
           time_t reference = timegm (&time_info);
           if (fileinfo.st_mtim.tv_sec <= reference) {
               echttp_error (304, "Not Modified");
               return "";
           }
        }
    }

    static char datestring[80];
    int formatted = strftime (datestring, sizeof(datestring),
                              "%a, %d %b %Y %H:%M:%S %Z",
                              gmtime (&(fileinfo.st_mtim.tv_sec)));
    if (formatted > 0) echttp_attribute_set ("Last-Modified", datestring);

    // Determine the mime type for that file.
    // Try using the extension first.
    // If there is no extension, or it is unknown, use the magic database.
    // If everything has failed, use some reasonable fallback value.
    //
    const char *content = 0;
    const char *sep = strrchr (filename, '.');
    if (sep)
        content = echttp_catalog_get (&echttp_static_type, sep+1);
    if ((!content) && echttp_magic_cookie)
        content = magic_file (echttp_magic_cookie, filename);
    if (!content)
        content = echttp_static_type_fallback ();
    echttp_content_type_set (content);

    // If the application contemplates creating pages of the fly, then
    // pages may change and we need the clients to revalidate their cache
    // every time.
    if (echttp_static_not_found != echttp_static_cannot_find)
        echttp_attribute_set ("Cache-Control", "no-cache");

    off_t size = fileinfo.st_size;

    // Support for partial content requests.
    //
    const char *rangespec = echttp_attribute_get ("Range");
    if (rangespec) {
        // This only supports a single range,
        // supported format is: 'bytes=' begin '-' [end].
        while (*rangespec == ' ') rangespec += 1;
        if (strncasecmp (rangespec, "bytes=", 6)) goto fullcontent;
        rangespec += 6;

        if (strchr (rangespec, ',')) goto fullcontent;
        const char *sep = strchr (rangespec, '-');
        if (!sep) goto fullcontent;

        off_t offset = atol (rangespec);
        if (offset < 0) goto fullcontent;

        off_t end = atoi (sep+1);
        if (end > 0) {
            if (end <= offset) goto fullcontent;
            size = end + 1 - offset;
        } else {
            size -= offset; // All the rest of the file.
        }

        if (offset > 0) {
            if (lseek (page, offset, SEEK_SET) != offset) goto unsupported;
        }
        if (size != fileinfo.st_size) {
            char ascii[128];
            snprintf (ascii, sizeof(ascii), "bytes %ld-%ld/%ld",
                      offset, offset+size-1, fileinfo.st_size);
            echttp_attribute_set ("Content-Range", ascii);
            echttp_error (206, "Partial Content"); // Not really an error.
        }
    }

fullcontent:
    echttp_transfer (page, size);
    return "";

unsupported:
    if (echttp_isdebug()) printf ("File type violation: %s\n", filename);
    echttp_error (406, "File Not Acceptable");
    close (page);
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

    snprintf (filename, sizeof(filename), "%s%s", path, uri+strlen(rooturi));

    return echttp_static_file (open (filename, O_RDONLY), filename);
}

static void echttp_static_internal_initialization (void) {

    static int Initialized = 0;
    if (!Initialized) {
        // Create some common default content types.
        int i;
        for (i = 0; echttp_static_default_types[i].extension; ++i) {
            echttp_catalog_set (&echttp_static_type,
                                echttp_static_default_types[i].extension,
                                echttp_static_default_types[i].content);
        }
        echttp_magic_cookie = magic_open (MAGIC_MIME_TYPE);
        Initialized = 1;
    }
}

echttp_not_found_handler *echttp_static_on_not_found (echttp_not_found_handler *handler) {
    echttp_not_found_handler *old = echttp_static_not_found;
    echttp_static_not_found = handler ? handler : echttp_static_cannot_find;
    return old;
}

void echttp_static_content_map (const char *extension, const char *content) {
    echttp_static_internal_initialization ();
    echttp_catalog_set (&echttp_static_type, extension, content);
}

int echttp_static_route (const char *uri, const char *path) {

    echttp_static_internal_initialization ();
    int route = echttp_route_find (uri);
    if (route >= 0) {
        const char *existing = echttp_catalog_get (&echttp_static_roots, uri);
        if (strcmp (existing, path)) {
            echttp_catalog_set (&echttp_static_roots, uri, path); // Changed.
        }
        return route;
    }
    echttp_catalog_set (&echttp_static_roots, uri, path);
    return echttp_route_match (uri, echttp_static_page); // All new URI.
}

static const char *EchttpStaticRoot = 0;

void echttp_static_default (const char *arg) {

    if (echttp_option_match ("-http-root=", arg, &EchttpStaticRoot)) return;
}

void echttp_static_initialize (int argc, const char *argv[]) {

    int i;
    for (i = 1; i < argc; ++i) {
        echttp_static_default (argv[i]); // Override the defaults.
    }
    echttp_static_internal_initialization ();
    if (EchttpStaticRoot) echttp_static_route ("/", EchttpStaticRoot);
}

