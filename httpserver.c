/* echttp - Embedded HTTP server.
 * A minimal HTTP server library designed for simplicity and embedding in
 * applications.
 *
 * httpserver.c -- a simple example on how to use echttp.
 */

#include <stdlib.h>
#include <stdio.h>

#include "echttp.h"

const char *http_welcome (const char *method, const char *uri,
                          const char *data, int length) {
    static char buffer[1024];
    const char *host = echttp_attribute_get("Host");
    if (host == 0) host = "(unknown)";
    echttp_attribute_set ("Content-Type", "text/html");
    snprintf (buffer, sizeof(buffer), "<e>Your are welcome on %s!</e>", host);
    return buffer;
}

const char *http_whoami (const char *method, const char *uri,
                         const char *data, int length) {
    echttp_attribute_set ("Content-Type", "text/html");
    return "<i>Who knows?</i>";
}

const char *http_echo (const char *method, const char *uri,
                         const char *data, int length) {
    static char buffer[1024];
    echttp_attribute_set ("Content-Type", "text/html");
    snprintf (buffer, sizeof(buffer),
              "<e>You called <b>%s</b></e> with %s",
              uri, echttp_parameter_get("what"));
    return buffer;
}

int main (int argc, const char **argv) {
    int i;
    argc = echttp_open (argc, argv);
    if (argc <= 0) exit(1);

    printf("Remaining arguments:\n");
    for (i = 1; i < argc; ++i) {
        printf ("  %d: %s\n", i, argv[i]);
    }
    echttp_route_uri ("/welcome", http_welcome);
    echttp_route_uri ("/whoami", http_whoami);
    echttp_route_match ("/echo", http_echo);

    echttp_loop();
    echttp_close();
}

