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
 * existing applications.
 *
 * echttp_open (int argc, const char **argv);
 *
 *    Initialize the HTTP server. The HTTP-specific arguments are removed
 *    from the argument list and the count of remaining arguments is returned.
 *
 *
 * typedef const char *echttp_callback (const char *method, const char *uri,
 *                                      const char *data, int length);
 *
 * int echttp_route_uri (const char *uri, echttp_callback *call);
 *
 * Define a route for processing the exact specified URI.
 *
 *
 * int echttp_route_match (const char *root, echttp_callback *call);
 *
 * Defines a route for a parent URI and all its children.
 *
 *
 * const char *echttp_attribute_get (const char *name);
 *
 * Get the value of the specified HTTP attribute, or 0 if not found.
 *
 *
 * const char *echttp_parameter_get  (const char *name);
 *
 * Get the value of the specified HTTP parameter, or 0 if not found.
 *
 *
 * void echttp_attribute_set (const char *name, const char *value);
 * 
 * Set an attribute for the HTTP response.
 * 
 * 
 * void echttp_error (int code, const char *message);
 * 
 * Send an error response instead of OK.
 * 
 * 
 * void echttp_redirect (const char *url);
 * 
 * Send a redirect response instead of OK.
 * 
 * 
 * typedef void *echttp_listener (int fd, int mode);
 * void echttp_listen (int fd, int mode, echttp_listener *listener);
 *
 * Listen to the specified file descriptor (mode=0: don't listen,
 * mode=1: read only, mode=2: write only, mode=3: read & write).
 *
 * When the specified file descriptor is ready, the listener is called
 * with the mode corresponding to the event.
 *
 *
 * void echttp_loop (void);
 * 
 * Enter the HTTP server main loop. The HTTP server may call any callback
 * or listener fucntion, in any order.
 * 
 *
 * int echttp_isdebug (void);
 *
 * Return true if the HTTP debug option was set.
 *
 * echttp_close (void);
 *
 *    Immediately close the HTTP server and all current HTTP connections.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp.h"
#include "echttp_raw.h"
#include "echttp_catalog.h"

static int echttp_debug = 0;


#define ECHTTP_STATE_IDLE    0
#define ECHTTP_STATE_CONTENT 1
typedef struct {
    int state;
    int route;
    int contentlength;
    char *method;
    char *uri;
    char *content;
    echttp_catalog in;
    echttp_catalog out;
    echttp_catalog params;

    int status;
    const char *reason;
} echttp_request;

static echttp_request *echttp_context = 0;
static int             echttp_context_count = 0;
static echttp_request *echttp_current = 0;

#define ECHTTP_MODE_EXACT   1
#define ECHTTP_MODE_PARENT  2

typedef struct {
    const char *uri;
    echttp_callback *call;
    unsigned int signature;
    int mode;
    int next;
} echttp_route;

#define ECHTTP_MAX_ROUTES 512
static struct {
    int count;
    int index[ECHTTP_HASH];
    echttp_route item[ECHTTP_MAX_ROUTES];
} echttp_routing;


static const char *echttp_match (const char *reference, const char *value) {
   int l = strlen(reference);
   if (strncmp (reference, value, l) == 0) {
      return value + l;
   }
   return 0;
}

static int echttp_split (char *data, const char *sep, char **items, int max) {
    int count = 0;
    int length = strlen(sep);
    char *start = data;

    while (*data) {
       if (strncmp(sep, data, length) == 0) {
           *data = 0;
           items[count++] = start;
           data += length;
           start = data;
       } else {
           data += 1;
       }
    }
    if (data > start) items[count++] = start;
    return count;
}


static void echttp_execute (int route, int client,
                           const char *action, const char *uri,
                           const char *data, int length) {
    static const char eol[] = "\r\n";

    int i;
    int keep;
    char buffer[256];

    echttp_current = &(echttp_context[client]);
    echttp_current->status = 200;
    echttp_current->reason = "OK";
    echttp_catalog_reset(&(echttp_current->out));

    const char *connection = echttp_attribute_get ("Connection");
    keep = (connection && (strcmp(connection, "keep-alive") == 0));

    data = echttp_routing.item[route].call (action, uri, data, length);
    length = data?strlen(data):0;

    snprintf (buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n",
             echttp_current->status, echttp_current->reason);
    echttp_raw_send (client, buffer, strlen(buffer), 0);

    if (keep) {
        static const char text[] = "Connection: keep-alive\r\n";
        echttp_raw_send (client, text, sizeof(text)-1, 0);
    }

    snprintf (buffer, sizeof(buffer)-1, "Content-Length: %d\r\n", length);
    buffer[sizeof(buffer)-1] = 0;
    echttp_raw_send (client, buffer, strlen(buffer), 0);

    for (i = 1; i <= echttp_current->out.count; ++i) {
        if (echttp_current->out.item[i].name == 0) continue;
        snprintf (buffer, sizeof(buffer)-1, "%s: %s\r\n",
                  echttp_current->out.item[i].name,
                  echttp_current->out.item[i].value);
        echttp_raw_send (client, buffer, strlen(buffer), 0);
    }
    echttp_raw_send (client, eol, sizeof(eol)-1, (data == 0 && keep == 0));
    if (data != 0) {
       echttp_raw_send (client, data, length, (keep == 0));
    }

    echttp_current = 0;
}

static void echttp_unknown (int client) {
    static const char unknown[] =
        "HTTP/1.1 404 Not found\r\n"
        "Content-Length: 0\r\n"
        "Connection: Closed\r\n\r\n";
    echttp_raw_send (client, unknown, sizeof(unknown)-1, 1);
}

static void echttp_invalid (int client) {
    static const char invalid[] =
        "HTTP/1.1 406 Not Acceptable\r\n"
        "Content-Length: 0\r\n"
        "Connection: Closed\r\n\r\n";
    echttp_raw_send (client, invalid, sizeof(invalid)-1, 1);
}

static int echttp_route_add (const char *uri, echttp_callback *call, int mode) {

    int i = echttp_routing.count + 1;
    unsigned int signature = echttp_catalog_signature (uri);
    int index = signature % ECHTTP_HASH;

    if (i >= ECHTTP_MAX_ROUTES) {
        fprintf (stderr, "Too many routes.\n");
        return -1;
    }
    echttp_routing.item[i].uri = uri;
    echttp_routing.item[i].call = call;
    echttp_routing.item[i].mode = mode;
    echttp_routing.item[i].signature = signature;
    echttp_routing.item[i].next = echttp_routing.index[index];
    echttp_routing.index[index] = i;
    echttp_routing.count = i;
    return i;
}

static int echttp_route_search (const char *uri, int mode) {
   int i;
   unsigned int signature = echttp_catalog_signature (uri);
   int index = signature % ECHTTP_HASH;

   if (echttp_debug) printf ("Searching route for %s\n", uri);
   for (i = echttp_routing.index[index];
        i > 0; i = echttp_routing.item[i].next) {
       if (echttp_debug)
           printf ("Matching with %s\n", echttp_routing.item[i].uri);
       if ((echttp_routing.item[i].mode == mode) &&
           (echttp_routing.item[i].signature == signature) &&
           strcmp (echttp_routing.item[i].uri, uri) == 0) return i;
   }
   return -1;
}

static int echttp_hextoi (char a) {
    if (isdigit(a)) return a - '0';
    return tolower(a) - 'a' + 10;
}

static char *echttp_unescape (char *data) {
    char *f = data;
    char *t = data;
    while (*f) {
        if (*f == '%') {
            if ((!isxdigit(f[1])) || (!isxdigit(f[2]))) return 0;
            *t = 16 * echttp_hextoi(f[1]) + echttp_hextoi(f[2]);
            f += 2;
        } else if (t != f) {
            *t = *f;
        }
        t += 1;
        f += 1;
    }
    *t = 0; // Force a terminator.
    return data;
}

static int echttp_received (int client, char *data, int length) {

   static const char endpattern[] = "\r\n\r\n";

   int i;
   char *endreq;
   char *enddata = data + length;
   int consumed = 0;

   if (client >= echttp_context_count) {
       echttp_context =
           realloc (echttp_context, (client+1) * sizeof(echttp_request));
       if (! echttp_context) {
           fprintf (stderr, "no more memory\n");
           exit(1);
       }
       for (i = echttp_context_count; i <= client; ++i) {
           echttp_context[i] = (echttp_request){0};
       }
       echttp_context_count = client + 1;
   }
   echttp_request *context = &(echttp_context[client]);

   if (echttp_debug) printf ("Received HTTP request (%d bytes)\n", length);
   data[length] = 0;

   // If there was content left to receive, and we just received it all,
   // execute the HTTP request. Otherwise wait for more.
   //
   if (context->state == ECHTTP_STATE_CONTENT) {
       if (context->contentlength > length) return 0; // Wait for more.
       echttp_execute (context->route, client,
                      context->method, context->uri,
                      data, context->contentlength);
       consumed = context->contentlength;
       data += context->contentlength;
       context->state = ECHTTP_STATE_IDLE;
   }

   // We are waiting for a new HTTP request.
   //
   while ((data < enddata) && ((endreq = strstr(data, endpattern)) != 0)) {
       // Decode this complete request.
       char *line[256];

       *endreq = 0;
       endreq += strlen(endpattern);;

       int linecount = echttp_split (data, "\r\n", line, 256);

       if (echttp_debug) printf("HTTP request: %s\n", line[0]);

       char *request[4];
       int wordcount = echttp_split (line[0], " ", request, 4);
       if (wordcount != 3) {
           echttp_invalid (client);
           return length; // Consume everything, since we are closing.
       }
       context->method = echttp_unescape(request[0]);
       context->uri = echttp_unescape(request[1]);
       if (context->method == 0 || context->uri == 0) {
           echttp_invalid (client);
           return length; // Consume everything, since we are closing.
       }

       echttp_catalog_reset(&(context->params));
       wordcount = echttp_split (context->uri, "?", request, 4);
       if (wordcount == 2) {
           char *arg[32];
           wordcount = echttp_split (request[1], "&", arg, 32);
           for (i = 0; i < wordcount; ++i) {
               char *param[4];
               if (echttp_split (arg[i], "=", param, 4) >= 2) {
                   echttp_catalog_set (&(context->params), param[0], param[1]);
               }
           }
       }

       // Search for a uri mapping: try exact match first, then for a parent.
       //
       i = echttp_route_search (context->uri, ECHTTP_MODE_EXACT);
       if (i <= 0) {
           char *uri = strdup(context->uri);
           char *sep = strrchr (uri+1, '/');
           while (sep) {
               *sep = 0;
               i = echttp_route_search (uri, ECHTTP_MODE_PARENT);
               if (i > 0) break;
               sep = strrchr (uri+1, '/');
           }
           free(uri);
           if (i <= 0) {
               echttp_unknown (client);
               return length; // Consume everything, since we are closing.
           }
       }
       context->route = i;

       // Decode the header parameters after the request line.
       //
       echttp_catalog_reset(&(context->in));
       for (i = 1; i < linecount; ++i) {
           char *param[4];
           if (echttp_split (line[i], ": ", param, 4) >= 2) {
               echttp_catalog_set (&(context->in), param[0], param[1]);
           }
       }

       // Make sure that all the content has been received.
       //
       const char *field = echttp_catalog_get(&(context->in), "Content-Length");
       context->contentlength = 0;
       context->content = endreq;
       if (field) {
          context->contentlength = atoi(field);
          if (context->contentlength > (int)(enddata - endreq)) {
              if (echttp_debug) printf("HTTP: waiting for end of content.\n");
              context->state = ECHTTP_STATE_CONTENT;
              return consumed; // Wait for more.
          }
       }
       if (!context->contentlength) context->content = 0;
       consumed += ((int) (endreq - data) + context->contentlength);
       data = endreq + context->contentlength;

       echttp_execute (context->route, client,
                      context->method, context->uri,
                      context->content, context->contentlength);
   }
   return consumed;
}

int echttp_open (int argc, const char **argv) {

   int i;
   int shift;
   const char *value;
   const char *service = "http";

   int port = -1;

   for (i = 1, shift = 1; i < argc; ++i) {
       if (shift != i) argv[shift] = argv[i];
       value = echttp_match ("-http-port=", argv[i]);
       if (value) {
           service = value;
           continue;
       }
       value = echttp_match ("-http-debug", argv[i]);
       if (value) {
           echttp_debug = 1;
           continue;
       }
       shift += 1;
   }
   echttp_routing.count = 0;
   if (! echttp_raw_open (service, echttp_debug)) return -1;
   return shift;
}

void echttp_loop (void) {

   echttp_raw_loop (echttp_received);
}

void echttp_close (void) {
   echttp_raw_close ();
}

int echttp_route_uri (const char *uri, echttp_callback *call) {
    return echttp_route_add (uri, call, ECHTTP_MODE_EXACT);
}

int echttp_route_match (const char *root, echttp_callback *call) {
    return echttp_route_add (root, call, ECHTTP_MODE_PARENT);
}

const char *echttp_attribute_get (const char *name) {
    return echttp_catalog_get (&(echttp_current->in), name);
}

const char *echttp_parameter_get (const char *name) {
    return echttp_catalog_get (&(echttp_current->params), name);
}

void echttp_attribute_set (const char *name, const char *value) {
    echttp_catalog_set (&(echttp_current->out), name, value);
}

void echttp_content_type_set (const char *value) {
    echttp_catalog_set (&(echttp_current->out), "Content-Type", value);
}

void echttp_error (int code, const char *message) {
    echttp_current->status = code;
    echttp_current->reason = message;
}

void echttp_redirect (const char *url) {
    echttp_error (301, "Redirected");
    echttp_attribute_set ("Location", url);
}

int echttp_isdebug (void) {
    return echttp_debug;
}

void echttp_listen (int fd, int mode, echttp_listener *listener, int premium) {
    echttp_raw_register (fd, mode, listener, premium);
}
 
