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
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "echttp.h"
#include "echttp_json.h"

#define MAXURLS 8
static char *Urls[MAXURLS];
static int UrlCount = 0;

static int Verbose = 0;

static void response (void *origin, int status, char *data, int length) {

    status = echttp_redirected ("GET");
    if (!status) {
        echttp_submit (0, 0, response, origin);
        return;
    }

    printf ("== HTTP code %d to %s: %s %s\n",
            status, (const char *)origin, echttp_reason(), Verbose?data:"");
}

static void background (int fd, int mode) {

    static int Count = 0;
    static time_t Pause = 0;

    time_t now = time(0);
    if ((Pause > 0) && (Pause > now)) return;

    if (++Count > 4) {
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
       if (strncmp (argv[i], "http", 4)) continue; // Not an URL
       Urls[UrlCount++] = strdup (argv[i]);
       if (UrlCount >= MAXURLS) break;
   }
       
   echttp_background (&background);
   echttp_loop ();
   return 0;
}

