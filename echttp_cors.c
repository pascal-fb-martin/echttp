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
 * A minimal implementation of the CORS mechanism
 *
 * This module implements the Cross-Origin Resource Sharing (CORS) HTTP
 * mechanism used to tell browsers if a cross origin access is allowed.
 *
 * SYNOPSYS:
 *
 * void echttp_cors_allow_method (const char *method);
 *
 *    Define which method or methods are allowed in a CORS case.
 *    This function should be called multiple times if more than
 *    one method is allowed.
 *
 * void echttp_cors_trust_origin (const char *name);
 *
 *    Defines one origin URL that is always trusted in a CORS case.
 *    This function should be called multiple times if more than
 *    one URL is allowed.
 *
 *    There can be two main reasons for trusting an URL:
 *    - The host name in this URL is trusted server, or
 *    - The host name is an alias of this same host.
 *
 * int echttp_cors_protect (const char *method, const char *uri);
 *
 *    This function must be called from a protect callback. It returns
 *    0 if the request processing may continue or 1 if the protect
 *    callback should return immediately.
 *
 *    In the later case, this function has setup the HTTP status and
 *    headers necessary: there is nothing more that the caller should do.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
// #include <ctype.h>
// #include <sys/types.h>
// #include <sys/stat.h>
// #include <fcntl.h>

#include "echttp.h"
#include "echttp_cors.h"

#define MAX_ALLOWED 64
typedef struct {
    int         count;
    const char *items[MAX_ALLOWED];
} echttp_allowed;

static echttp_allowed echttp_allowed_methods = {0};
static echttp_allowed echttp_allowed_origin = {0};

static char  echttp_all_allowed_methods[1024] = {0};


static void echttp_cors_initialize (void) {

    if (echttp_allowed_origin.count == 0) {
        // Always allow this local host, either under its name or 'localhost'..
        char hostname[256];
        char buffer[256];
        gethostname (hostname, sizeof(hostname));
        snprintf (buffer, sizeof(buffer),
                  "http://%s:%d", hostname, echttp_port(4));
        if (echttp_isdebug()) printf ("Local server is %s\n", buffer);
        echttp_allowed_origin.items[0] = strdup(buffer);
        snprintf (buffer, sizeof(buffer), "http://%s", hostname);
        echttp_allowed_origin.items[1] = strdup(buffer);
        snprintf (buffer, sizeof(buffer),
                  "http://localhost:%d", echttp_port(4));
        echttp_allowed_origin.items[2] = strdup(buffer);
        echttp_allowed_origin.count = 3;
    }
}

static void echttp_cors_allow (const char *item,
                               echttp_allowed *allowed, const char *label) {

    echttp_cors_initialize ();

    if (allowed->count >= MAX_ALLOWED) return;

    if (echttp_isdebug()) printf ("Allowing %s %s\n", label, item);
    allowed->items[allowed->count++] = strdup(item);
}

void echttp_cors_allow_method (const char *method) {

    int length = strlen(echttp_all_allowed_methods);
    const char *format = (length > 0) ? ", %s" : "%s";

    echttp_cors_allow (method, &echttp_allowed_methods, "method");

    snprintf (echttp_all_allowed_methods+length,
              sizeof(echttp_all_allowed_methods)-length,
              format, method);
}

void echttp_cors_trust_origin (const char *url) {

    echttp_cors_allow (url, &echttp_allowed_origin, "URL");
}

static int echttp_cors_allowed (const char *item,
                                const echttp_allowed *allowed) {
    int i;

    if (!item) return 0;
    for (i = 0; i < allowed->count; ++i) {
        if (!strcmp(item, allowed->items[i])) return 1;
    }
    return 0;
}

int echttp_cors_protect (const char *method, const char *uri) {

    echttp_cors_initialize();

    const char *origin = echttp_attribute_get ("Origin");
    if (!origin) return 0; // Not a cross-domain request.

    if (! echttp_cors_allowed (origin, &echttp_allowed_origin)) {

        // If this is not one of these trusted origins, let check
        // if the method is one allowed to others.
        //
        if (!strcmp (method, "OPTIONS")) { // Preflight request.

            method = echttp_attribute_get ("Access-Control-Request-Method");
            if (!echttp_cors_allowed (method, &echttp_allowed_methods)) {
                echttp_error (403, "Forbidden Cross-Domain");
                return 1;
            }
            echttp_attribute_set ("Access-Control-Allow-Origin", "*");
            echttp_attribute_set ("Access-Control-Allow-Methods",
                                  echttp_all_allowed_methods);
            echttp_error (204, "No Content"); // Not an error but stop here.
            return 1;
        }

        if (!echttp_cors_allowed (method, &echttp_allowed_methods)) {
            echttp_error (403, "Forbidden Cross-Domain");
            return 1;
        }
    }

    echttp_attribute_set ("Access-Control-Allow-Origin", "*");
    return 0; // Keep going.
}

