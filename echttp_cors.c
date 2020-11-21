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
 *    one method is allowed. The method string must be static.
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

#define MAX_METHOD 64
static const char *echttp_allowed[MAX_METHOD];
static int         echttp_allowed_count = 0;
static char        echttp_all_allowed[1024] = {0};

void echttp_cors_allow_method (const char *method) {

    if (echttp_allowed_count >= MAX_METHOD) return;

    echttp_allowed[echttp_allowed_count++] = method;
    if (echttp_all_allowed[0]) {
        int length = strlen(echttp_all_allowed);
        snprintf (echttp_all_allowed+length, sizeof(echttp_all_allowed)-length,
                  ", %s", method);
    } else {
        snprintf (echttp_all_allowed, sizeof(echttp_all_allowed), "%s", method);
    }
}

static int echttp_cors_reject (const char *method) {
    int i;

    if (!method) return 1;
    for (i = 0; i < echttp_allowed_count; ++i) {
        if (!strcmp(method, echttp_allowed[i])) return 0;
    }
    return 1;
}

int echttp_cors_protect (const char *method, const char *uri) {

    const char *origin = echttp_attribute_get ("Origin");
    if (!origin) return 0; // Not a cross-domain request.

    if (!strcmp (method, "OPTIONS")) { // Preflight request.

        method = echttp_attribute_get ("Access-Control-Request-Method");
        if (echttp_cors_reject (method)) {
            echttp_error (403, "Forbidden Cross-Domain");
            return 1;
        }
        echttp_attribute_set ("Access-Control-Allow-Origin", "*");
        echttp_attribute_set ("Access-Control-Allow-Methods",
                              echttp_all_allowed);
        echttp_error (204, "No Content"); // Not an error but stop here.
        return 1;
    }

    if (echttp_cors_reject (method)) {
        echttp_error (403, "Forbidden Cross-Domain");
        return 1;
    }
    echttp_attribute_set ("Access-Control-Allow-Origin", "*");
    return 0; // Keep going.
}

