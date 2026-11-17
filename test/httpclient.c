/* echttp - Embedded HTTP server.
 *
 * Copyright 2026, Pascal Martin
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
 * httpclient.c: a test program to validate the connection reuse mechanism
 *
 * SYNOPSYS:
 *
 * httpclient [-c] [-v] URL ..
 *
 * This application periodically initiates all the listed requests (URLs)
 * from its command lines, in successive short bursts separated by a 15
 * seconds pause.
 *
 * The -c option causes httpclient to show a list of the current connection
 * context held by the echttp library. The list is shown when the periodic
 * pause starts.
 *
 * The -v option causes httpclient to show the content portion of the HTTP
 * responses. This can be lengthy.
 */

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "echttp.h"
#include "echttp_json.h"

#include "../echttp_raw.h"

#define MAXURLS 8
static char *Urls[MAXURLS];
static int UrlCount = 0;

static int Verbose = 0;
static int ShowConnections = 0;

static void response (void *origin, int status, char *data, int length) {

    status = echttp_redirected ("GET");
    if (!status) {
        echttp_submit (0, 0, response, origin);
        return;
    }

    printf ("== %s: HTTP status %d %s %s\n",
            (const char *)origin, status, echttp_reason(), Verbose?data:"");
}

static void background (int fd, int mode) {

    static int Count = 0;
    static time_t Pause = 0;

    time_t now = time(0);
    if ((Pause > 0) && (Pause > now)) return;

    if (++Count > 4) {
        if (ShowConnections && (Count == 5)) {
           const char *side;
           char peer[256];
           long long sent, received;
           int client = echttp_raw_walk (-1,
                                         &side, peer, sizeof(peer),
                                         &sent, &received);
           while (client >= 0) {
               printf ("Client[%d]: %s connected to %s, %lld sent, %lld received\n",
                       client, side, peer, sent, received);
               client = echttp_raw_walk (client,
                                         &side, peer, sizeof(peer),
                                         &sent, &received);
           }
        }
        Pause = now + 15;
        Count = 0;
        return;
    }

    int i;
    for (i = 0; i < UrlCount; ++i) {
        const char *error = echttp_client ("GET", Urls[i]);
        if (error) {
            printf ("** echttp_client() error: %s\n", error);
            continue;
        }
        echttp_submit (0, 0, response, Urls[i]);
    }
}

int main (int argc, const char **argv) {

   echttp_default ("--http-service=dynamic");
   argc = echttp_open (argc, argv);

   int i;
   for (i = 1; i < argc; ++i) {
       if (!strcmp (argv[i], "-v")) {
           Verbose = 1;
           continue;
       }
       if (!strcmp (argv[i], "-c")) {
           ShowConnections = 1;
           continue;
       }
       if (!strncmp (argv[i], "http", 4)) {
           Urls[UrlCount++] = strdup (argv[i]);
           if (UrlCount >= MAXURLS) break;
       }
   }

   echttp_background (&background);
   echttp_loop ();
   return 0;
}

