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

static void http_protected (const char *method, const char *uri) {
    printf ("%s %s was protected.\n", method, uri);
    if (strcmp (uri, "/forbidden") == 0) {
        echttp_error (401, "Unauthorized");
    }
}

static const char *http_forbidden (const char *method, const char *uri,
                                   const char *data, int length) {
    return "<e>This is protected content!</e>";
}

static const char *http_welcome (const char *method, const char *uri,
                                 const char *data, int length) {
    static char buffer[1024];
    const char *host = echttp_attribute_get("Host");
    if (host == 0) host = "(unknown)";
    echttp_content_type_set ("text/html");
    snprintf (buffer, sizeof(buffer), "<e>Your are welcome on %s!</e>", host);
    return buffer;
}

static const char *http_whoami (const char *method, const char *uri,
                                const char *data, int length) {
    echttp_content_type_set ("text/html");
    return "<i>Who knows?</i>";
}

static const char *http_echo (const char *method, const char *uri,
                              const char *data, int length) {
    static char buffer[1024];
    echttp_content_type_set ("text/html");
    snprintf (buffer, sizeof(buffer),
              "<e>You called <b>%s</b></e> with what = %s",
              uri, echttp_parameter_get("what"));
    return buffer;
}

static void http_response (void *origin, int status, char *data, int length) {
    if (echttp_isdebug()) printf ("HTTP response status %d\n", status);
    if (status == 302) { // Redirected
        const char *to = echttp_attribute_get("Location");
        const char *error;
        if (echttp_isdebug()) printf ("HTTP redirected to: %s\n", to);
        error = echttp_client ("GET", to);
        if (error)
            printf ("%s: %s\n", to, error);
        else
            echttp_submit (0, 0, http_response, 0);
        return;
    }
    if (length > 0)
        printf ("%*.*s\n", length, length, data);
}

static void http_console (int fd, int mode) {
    char *eol;
    char buffer[1024];
    fgets(buffer, sizeof(buffer), stdin);
    eol = strchr (buffer, '\n');
    if (eol) *eol = 0;
    if (echttp_isdebug()) printf ("Console: %s\n", buffer);
    if (strcmp (buffer, "exit") == 0) {
        echttp_close();
        exit(0);
    }
    if (strncmp(buffer, "get ", 4) == 0) {
        if (echttp_isdebug()) printf ("HTTP request GET %s\n", buffer+4);
        const char *error = echttp_client ("GET", buffer+4);
        if (error) {
            printf ("%s: %s\n", buffer+4, error);
            return;
        }
        echttp_submit (0, 0, http_response, 0);
    }
}

int main (int argc, const char **argv) {
    int i;
    argc = echttp_open (argc, argv);
    if (argc <= 0) exit(1);

    if (echttp_isdebug()) {
        if (argc <= 1)
           printf("No remaining argument\n");
        else {
           printf("Remaining arguments:\n");
           for (i = 1; i < argc; ++i) {
               printf ("  %d: %s\n", i, argv[i]);
           }
        }
    }
    echttp_protect
        (echttp_route_uri ("/welcome", http_welcome), http_protected);
    echttp_route_uri ("/whoami", http_whoami);
    echttp_protect
        (echttp_route_uri ("/forbidden", http_forbidden), http_protected);
    echttp_route_match ("/echo", http_echo);
    echttp_static_route ("/", getcwd(0, 0));
    echttp_static_route ("/static", getcwd(0, 0));

    echttp_listen (0, 1, http_console, 1);

    echttp_loop();
}

