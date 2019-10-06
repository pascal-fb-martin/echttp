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
 * A minimal HTTP server library designed for simplicity and embedding in
 * applications.
 *
 * httpserver.c -- a simple example on how to use echttp.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_static.h"

const char *http_welcome (const char *method, const char *uri,
                          const char *data, int length) {
    static char buffer[1024];
    const char *host = echttp_attribute_get("Host");
    if (host == 0) host = "(unknown)";
    echttp_content_type_set ("text/html");
    snprintf (buffer, sizeof(buffer), "<e>Your are welcome on %s!</e>", host);
    return buffer;
}

const char *http_whoami (const char *method, const char *uri,
                         const char *data, int length) {
    echttp_content_type_set ("text/html");
    return "<i>Who knows?</i>";
}

const char *http_echo (const char *method, const char *uri,
                         const char *data, int length) {
    static char buffer[1024];
    echttp_content_type_set ("text/html");
    snprintf (buffer, sizeof(buffer),
              "<e>You called <b>%s</b></e> with what = %s",
              uri, echttp_parameter_get("what"));
    return buffer;
}

static void http_console (int fd, int mode) {
    char buffer[1024];
    fgets(buffer, sizeof(buffer), stdin);
    if (echttp_isdebug()) printf ("Console: %s\n", buffer);
    if (strncmp (buffer, "exit\n", 5) == 0) {
        echttp_close();
        exit(0);
    }
}

int main (int argc, const char **argv) {
    int i;
    argc = echttp_open (argc, argv);
    if (argc <= 0) exit(1);

    if (echttp_isdebug()) {
        printf("Remaining arguments:\n");
        for (i = 1; i < argc; ++i) {
            printf ("  %d: %s\n", i, argv[i]);
        }
    }
    echttp_route_uri ("/welcome", http_welcome);
    echttp_route_uri ("/whoami", http_whoami);
    echttp_route_match ("/echo", http_echo);
    echttp_static_route ("/static", getcwd(0, 0));

    echttp_listen (0, 1, http_console, 1);

    echttp_loop();
    echttp_close();
}

