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
 * void echttp_default (const char *arg);
 *
 *    Set a default value for a command line option. The parameters must
 *    follow the exact same syntax as for command line options, i.e. it
 *    must be in the form "-option=value" or "-option". This function
 *    allows an application to override the echttp's own hardcoded
 *    option defaults and force its own.
 *
 *    This can be called multiple times, and it must be before echttp_open().
 *
 * int echttp_open (int argc, const char **argv);
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
 * typedef void echttp_protect_callback (const char *method, const char *uri);
 *
 * int echttp_protect (int route, echttp_protect_callback *call);
 *
 * Define a protect callback for the specified route.
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
 * const char *echttp_reason (void);
 *
 * Return the current reason message for the current request. This function
 * simply repeats the last message string that was set through echttp_error().
 *
 *
 * void echttp_redirect (const char *url);
 * 
 * Send a temporary redirect response instead of OK.
 * 
 *
 * int echttp_islocal (void);
 *
 * Return 1 if the current client is on a local network.
 *
 * int echttp_port (int ip);
 *
 * Return the web server's port number for IPv4 (ip==4) or IPv6 (ip==6).
 * If the port number is 0, the web server is not listening on the specified
 * IP namespace.
 *
 * int echttp_dynamic_port (void);
 *
 * Return true if the HTTP server uses a dynamic port, false otherwise.
 * Dynamic ports are typically used when multiple HTTP servers run on
 * the same machine (e.g. micro services), but require using a discovery
 * service (e.g. houseportal). Dynamic port mode is activated using the
 * command line option -http-service=dynamic.
 *
 * int echttp_connect (const char *host, const char *service);
 *
 * A simple helper for establishing a TCP connection. The returned socket
 * has not been registered for listening: the application would have to
 * call echttp_listen().
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
 * void echttp_background (echttp_listener *listener);
 *
 * Call this listener completing I/O operations, before waiting for
 * new I/O. This background listener is called with fd 0 and mode 0,
 * and should not block on I/O itself.
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
 *
 * Web client functions:
 *
 * void echttp_escape (const char *s, char *d, int size);
 *
 *    Encode an HTTP parameter value.
 *
 * const char *echttp_client (const char *method, const char *url);
 *
 *    Establish a new web client context. Return a null pointer on success
 *    or an error string on failure. At this point the application may set
 *    attributes or set up a file for transfer before the last step: sending
 *    the request.
 *
 * typedef void echttp_response
 *                 (void *origin, int status, char *data, int length);
 *
 * void echttp_submit (const char *data, int length,
 *                     echttp_response *response, void *origin);
 *
 *    Send the web request for the current web client context. This closes
 *    the web client context. When a response will be received from the
 *    server, the response function will be called, with the provided
 *    origin pointer as its first parameter.
 *
 * int echttp_redirected (const char *method);
 *
 *    This helper function handles the common cases of redirections.
 *    If used, this must be called from the application's response callback,
 *    and it must be the first thing that the response callback do. How the
 *    response callback then behaves depends on the return code:
 *       >0: the response was not handled and the response must be processed
 *           as normal. The value returned is an updated HTTP status.
 *       0:  the response was handled and the response callback must now
 *           (re)build the request data, call echttp_submit and return.
 *    The method parameter is the method used for the original request.
 *    Depending on the redirect code, either this method will be used or
 *    else a GET method will be forced.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp.h"
#include "echttp_raw.h"
#include "echttp_tls.h"
#include "echttp_catalog.h"
#include "echttp_encoding.h"

static const char *echttp_service = "http";
static int         echttp_debug = 0;


#define ECHTTP_STATE_IDLE    0
#define ECHTTP_STATE_CONTENT 1

enum echttp_transport {ECHTTP_RAW = 0, ECHTTP_TLS = 1};

typedef struct {
    enum echttp_transport mode;
    int state;
    int route;
    int client;
    int contentlength;
    char *method;
    char *uri;
    char *content;
    echttp_catalog in;
    echttp_catalog out;
    echttp_catalog params;

    int status;
    const char *reason;

    struct {
        int fd;
        int size;
    } transfer;

    echttp_response *response;
    void *origin;

} echttp_request;

static echttp_request **echttp_context = 0;
static int              echttp_context_count = 0;
static echttp_request *echttp_current = 0;

static int echttp_dynamic_flag = 0;

#define ECHTTP_MODE_EXACT   1
#define ECHTTP_MODE_PARENT  2
#define ECHTTP_MODE_ANY     (ECHTTP_MODE_EXACT|ECHTTP_MODE_PARENT)

typedef struct {
    const char *uri;
    echttp_callback *call;
    echttp_protect_callback *protect;
    unsigned int signature;
    int mode;
    int next;
} echttp_route;

#define ECHTTP_MAX_ROUTES 512
static struct {
    int count;
    int index[ECHTTP_HASH];
    echttp_route item[ECHTTP_MAX_ROUTES];
    echttp_protect_callback *protect;
} echttp_routing;


static int echttp_split (char *data, const char *sep, char **items, int max) {
    int count = 0;
    int length = strlen(sep);
    char *start = data;

    while (*data) {
       if (strncmp(sep, data, length) == 0) {
           *data = 0;
           if (count >= max) return count;
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


static void echttp_send (int client, const char *data, int length) {
    switch (echttp_context[client]->mode) {
        case ECHTTP_RAW:
            echttp_raw_send (client, data, length);
            break;
        case ECHTTP_TLS:
            echttp_tls_send (client, data, length);
            break;
    }
}

static void echttp_send_data (int client, const char *data, int length) {

    echttp_request *context = echttp_context[client];
    static const char eol[] = "\r\n";
    int i;
    char buffer[256];

    snprintf (buffer, sizeof(buffer)-1, "Content-Length: %d\r\n",
              length + context->transfer.size);
    echttp_send (client, buffer, strlen(buffer));

    for (i = 1; i <= context->out.count; ++i) {
        if (context->out.item[i].name == 0) continue;
        snprintf (buffer, sizeof(buffer)-1, "%s: %s\r\n",
                  context->out.item[i].name,
                  (char *)(context->out.item[i].value));
        echttp_send (client, buffer, strlen(buffer));
    }
    echttp_send (client, eol, sizeof(eol)-1);
    if (length > 0) {
       echttp_send (client, data, length);
    }
    if (context->transfer.size > 0) {
        // This transfer must be submitted to the raw layer only after
        // all the preamble was submitted. Otherwise the raw layer may
        // start the file transfer before the HTTP preamble was sent..
        //
        echttp_raw_transfer (client,
                             context->transfer.fd,
                             context->transfer.size);
        context->transfer.fd = -1;
        context->transfer.size = 0;
    }
}

static void echttp_execute (int route, int client,
                           const char *action, const char *uri,
                           const char *data, int length) {

    char buffer[256];
    echttp_request *context = echttp_context[client];

    // Do not rely on echttp_current internally: the application is allowed
    // to submit a client request in reaction to the received request,
    // so the current context might have been replaced.
    //
    const char *connection = echttp_catalog_get (&(context->in), "Connection");
    int keep = (connection && (strcmp(connection, "keep-alive") == 0));

    context->status = 200;
    context->reason = "OK";
    echttp_catalog_reset(&(context->out));

    context->transfer.fd = -1;
    context->transfer.size = 0;

    echttp_current = context;
    if (echttp_routing.protect) {
        echttp_routing.protect (action, uri);
    }
    if (context->status == 200) {
        if (echttp_routing.item[route].protect) {
            echttp_routing.item[route].protect (action, uri);
        }
    }
    if (context->status == 204) {
        data = 0;
    } else if ((context->status / 100) != 2) {
        keep = 0;
        data = 0;
    } else {
        data = echttp_routing.item[route].call (action, uri, data, length);
    }
    echttp_current = 0;

    length = data?strlen(data):0;

    snprintf (buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n",
             context->status, context->reason);
    echttp_send (client, buffer, strlen(buffer));

    if (keep) {
        static const char text[] = "Connection: keep-alive\r\n";
        echttp_send (client, text, sizeof(text)-1);
    }

    echttp_send_data (client, data, length);
}

static void echttp_unknown (int client) {
    static const char unknown[] =
        "HTTP/1.1 404 Not found\r\n"
        "Content-Length: 0\r\n"
        "Connection: Closed\r\n\r\n";
    echttp_send (client, unknown, sizeof(unknown)-1);
}

static void echttp_invalid (int client, const char *text) {
    static const char invalidformat[] =
        "HTTP/1.1 406 Invalid %s\r\n"
        "Content-Length: 0\r\n"
        "Connection: Closed\r\n\r\n";
    char invalid[1024];
    int length = snprintf (invalid, sizeof(invalid), invalidformat, text);
    if (length >= sizeof(invalid)) length = sizeof(invalid) - 1;
    echttp_send (client, invalid, length);
}

static int echttp_route_add (const char *uri, echttp_callback *call, int mode) {

    int i = echttp_routing.count + 1;
    unsigned int signature = echttp_hash_signature (uri);
    int index = signature % ECHTTP_HASH;

    if (i >= ECHTTP_MAX_ROUTES) {
        fprintf (stderr, "Too many routes.\n");
        return -1;
    }
    echttp_routing.item[i].uri = uri;
    echttp_routing.item[i].call = call;
    echttp_routing.item[i].protect = 0;
    echttp_routing.item[i].mode = mode;
    echttp_routing.item[i].signature = signature;
    echttp_routing.item[i].next = echttp_routing.index[index];
    echttp_routing.index[index] = i;
    echttp_routing.count = i;
    return i;
}

static int echttp_route_search (const char *uri, int mode) {
   int i;
   unsigned int signature = echttp_hash_signature (uri);
   int index = signature % ECHTTP_HASH;

   static char *toascii[] = {"(invalid)", "exact", "parent", "any"};

   if (echttp_debug)
       printf ("Searching route for %s (mode %s)\n", uri, toascii[mode]);
   for (i = echttp_routing.index[index];
        i > 0; i = echttp_routing.item[i].next) {
       if ((echttp_routing.item[i].mode & mode) == 0) continue;
       if (echttp_debug)
           printf ("Matching with %s (%s entry)\n",
                   echttp_routing.item[i].uri,
                   toascii[echttp_routing.item[i].mode]);
       if ((echttp_routing.item[i].signature == signature) &&
           strcmp (echttp_routing.item[i].uri, uri) == 0) return i;
   }
   return -1;
}


static void echttp_respond (int client, char *data, int length) {

   // Do not rely on echttp_current internally: this is processing a response
   // and there is no current request processing.
   //
   echttp_request *context = echttp_context[client];

   echttp_current = context;
   context->response (context->origin, context->status, data, length);
   echttp_current = 0;

   context->response = 0;
   context->origin = 0;
   echttp_catalog_reset(&(context->in));
}

static int echttp_newclient (int client) {

   if (client >= echttp_context_count) {
       fprintf (stderr, "Invalid client context\n");
       return -1;
   }
   if (echttp_debug) printf ("New client %d is reported\n", client);

   echttp_request *context = echttp_context[client];
   if (!context) {
       context = echttp_context[client] = malloc (sizeof(echttp_request));
       context->client = client;
   }
   context->state = ECHTTP_STATE_IDLE;
   context->mode = ECHTTP_RAW;
   context->transfer.fd = -1;
   context->transfer.size = 0;
   context->response = 0;
   context->origin = 0;
   return 1;
}

static int echttp_received (int client, char *data, int length) {

   static const char endpattern[] = "\r\n\r\n";

   int i;
   char *endreq;
   char *enddata = data + length;
   int consumed = 0;
   echttp_request *context = echttp_context[client];

   if (length < 0) {
       // This reports a TCP connection error.
       if (echttp_debug)
           printf ("End of connection while waiting for %s\n",
                   context->response?"response":"request");
       if (context->response) {
           context->status = 505;
           echttp_respond (client, 0, 0);
       }
       return 0; // The connection will be closed by echttp_raw.
   }

   if (echttp_debug)
       printf ("Received HTTP %s (%d bytes)\n",
               echttp_context[client]->response?"response":"request", length);
   data[length] = 0;

   // If there was content left to receive, and we just received it all,
   // execute the HTTP request. Otherwise wait for more.
   //
   if (context->state == ECHTTP_STATE_CONTENT) {
       if (context->contentlength > length) return 0; // Wait for more.
       if (context->response) {
           echttp_respond (client, data, context->contentlength);
           echttp_raw_close_client(context->client, "end of response");
           return 0; // Connection was closed, nothing more to do.
       }
       echttp_execute (context->route, client,
                       context->method, context->uri,
                       data, context->contentlength);
       consumed = context->contentlength;
       data += context->contentlength;
       context->state = ECHTTP_STATE_IDLE;
   }

   // We are waiting for a new HTTP PDU.
   //
   while ((data < enddata) && ((endreq = strstr(data, endpattern)) != 0)) {
       // Decode this complete HTTP header.
       char *line[256];

       *endreq = 0;
       endreq += strlen(endpattern);;

       int linecount = echttp_split (data, "\r\n", line, 256);

       if (context->response) {
           // Expect a status line.
           if (echttp_debug) printf("HTTP status: %s\n", line[0]);
           if (strncmp (line[0], "HTTP/1.", 7)) {
               context->status = 505;
               echttp_respond (client, 0, 0);
               echttp_raw_close_client(context->client, "protocol error");
               return 0; // Connection was closed, nothing more to do.
           }
           context->status = atoi(strchr(line[0], ' '));
           if (context->status < 100 || context->status >= 600) context->status = 500;
       } else {
           // Expect a request line.
           if (echttp_debug) printf("HTTP request: %s\n", line[0]);

           char *request[4];
           char *rawuri[4];
           int wordcount = echttp_split (line[0], " ", request, 4);
           if (wordcount != 3) {
               echttp_invalid (client, "HTTP Request Line");
               return length; // Consume everything, since we are closing.
           }
           wordcount = echttp_split (request[1], "?", rawuri, 4);
           context->method = echttp_encoding_unescape(request[0]);
           context->uri = echttp_encoding_unescape(rawuri[0]);

           if (context->method == 0) {
               echttp_invalid (client, "HTTP Method");
               return length; // Consume everything, since we are closing.
           }
           if (context->uri == 0) {
               echttp_invalid (client, "HTTP URI");
               return length; // Consume everything, since we are closing.
           }

           echttp_catalog_reset(&(context->params));
           if (wordcount == 2) {
               char *arg[32];
               wordcount = echttp_split (rawuri[1], "&", arg, 32);
               for (i = 0; i < wordcount; ++i) {
                   char *param[4];
                   if (echttp_split (arg[i], "=", param, 4) >= 2) {
                       char *name = echttp_encoding_unescape (param[0]);
                       char *value = echttp_encoding_unescape (param[1]);
                       if (!name || !value) {
                           echttp_invalid (client, "HTTP Parameter Syntax");
                           return length; // Consume everything, invalid.
                       }
                       echttp_catalog_set (&(context->params), name, value);
                   }
               }
           }

           // Search for a uri mapping: try any match first, then for a parent.
           //
           i = echttp_route_search (context->uri, ECHTTP_MODE_ANY);
           if (i <= 0) {
               char *uri = strdup(context->uri);
               char *sep = strrchr (uri+1, '/');
               while (sep) {
                   *sep = 0;
                   i = echttp_route_search (uri, ECHTTP_MODE_PARENT);
                   if (i > 0) break;
                   sep = strrchr (uri+1, '/');
               }
               if (i <= 0) {
                   i = echttp_route_search ("/", ECHTTP_MODE_PARENT);
               }
               free(uri);
               if (i <= 0) {
                   echttp_unknown (client);
                   return length; // Consume everything, since we are closing.
               }
           }
           context->route = i;
       }

       // Decode the header parameters after the request/status line.
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
          if (context->contentlength < 0) context->contentlength = 0;
          if (context->contentlength > (int)(enddata - endreq)) {
              if (echttp_debug) printf("HTTP: waiting for end of content.\n");
              context->state = ECHTTP_STATE_CONTENT;
              consumed += ((int) (endreq - data));
              return consumed; // Wait for more.
          }
       }
       if (!context->contentlength) context->content = 0;
       consumed += ((int) (endreq - data) + context->contentlength);
       data = endreq + context->contentlength;

       if (context->response) {
           echttp_respond (client, context->content, context->contentlength);
           echttp_raw_close_client(context->client, "end of response");
           return 0; // Nothing more to do with this connection.
       }
       echttp_execute (context->route, client,
                      context->method, context->uri,
                      context->content, context->contentlength);
   }
   return consumed;
}

static void echttp_terminate (int client, const char *reason) {
    if (!echttp_context[client]) return;
    switch (echttp_context[client]->mode) {
        case ECHTTP_RAW:
            break;
        case ECHTTP_TLS:
            echttp_tls_detach_client (client, reason);
            break;
    }
}

const char *echttp_help (int level) {
    static const char *httpHelp[] = {
        " [-http-service=NAME] [-http-debug]",
        "-http-service=NAME:  name or port number for the HTTP socket (http).",
        "-http-debug:         enable debug traces.",
        NULL
    };

    return httpHelp[level];
}

void echttp_default (const char *arg) {

   if (echttp_option_match ("-http-service=", arg, &echttp_service)) return;

   if (echttp_option_present ("-http-debug", arg)) {
      echttp_debug = 1;
      return;
   }
}

int echttp_open (int argc, const char **argv) {

   int i;
   int shift;
   int ttl = 0;
   const char *ttl_ascii;

   for (i = 1, shift = 1; i < argc; ++i) {
       if (shift != i) argv[shift] = argv[i];
       if (echttp_option_match ("-http-service=", argv[i], &echttp_service))
           continue;
       if (echttp_option_match ("-http-ttl=", argv[i], &ttl_ascii)) {
           ttl = atoi(ttl_ascii);
           if (ttl < 0) ttl = 0;
           continue;
       }
       if (echttp_option_present ("-http-debug", argv[i])) {
           echttp_debug = 1;
           continue;
       }
       shift += 1;
   }
   echttp_routing.count = 0;
   echttp_routing.protect = 0;
   if (! echttp_raw_open (echttp_service, echttp_debug, ttl)) return -1;
   echttp_context_count = echttp_raw_capacity();
   echttp_context = calloc (echttp_context_count, sizeof(echttp_request *));
   echttp_dynamic_flag = (strcmp(echttp_service, "dynamic") == 0);

   return echttp_tls_initialize (echttp_context_count, shift, argv);
}

void echttp_loop (void) {
   echttp_raw_loop (echttp_newclient, echttp_received, echttp_terminate);
   echttp_raw_close ();
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

int echttp_protect (int route, echttp_protect_callback *call) {
    if (route < 0 || route > echttp_routing.count) return -1;
    if (route == 0)
        echttp_routing.protect = call;
    else
        echttp_routing.item[route].protect = call;
    return route;
}

const char *echttp_attribute_get (const char *name) {
    return echttp_catalog_get (&(echttp_current->in), name);
}

const char *echttp_parameter_get (const char *name) {
    return echttp_catalog_get (&(echttp_current->params), name);
}

void echttp_parameter_join (char *text, int size) {
    echttp_catalog_join (&(echttp_current->params), "&", text, size);
}

void echttp_attribute_set (const char *name, const char *value) {
    echttp_catalog_set (&(echttp_current->out), name, value);
}

void echttp_content_type_set (const char *value) {
    echttp_catalog_set (&(echttp_current->out), "Content-Type", value);
}

void echttp_content_type_json (void) {
    echttp_catalog_set (&(echttp_current->out),
                        "Content-Type", "application/json");
}

void echttp_content_type_html (void) {
    echttp_catalog_set (&(echttp_current->out), "Content-Type", "text/html");
}

void echttp_content_type_css (void) {
    echttp_catalog_set (&(echttp_current->out), "Content-Type", "text/css");
}

void echttp_transfer (int fd, int size) {
    if (echttp_current->transfer.size <= 0) {
        echttp_current->transfer.fd = fd;
        echttp_current->transfer.size = size;
    }
}

void echttp_error (int code, const char *message) {
    echttp_current->status = code;
    echttp_current->reason = message;
}

const char *echttp_reason (void) {
    return echttp_current->reason;
}

void echttp_redirect (const char *url) {
    echttp_error (307, "Temporary Redirect");
    echttp_attribute_set ("Location", url);
}

void echttp_permanent_redirect (const char *url) {
    echttp_error (308, "Redirected permanently");
    echttp_attribute_set ("Location", url);
}

int echttp_islocal (void) {
    return echttp_raw_is_local(echttp_current->client);
}

int echttp_port (int ip) {
    return echttp_raw_server_port (ip);
}

int echttp_dynamic_port (void) {
   return echttp_dynamic_flag;
}

int echttp_isdebug (void) {
    return echttp_debug;
}

int echttp_connect (const char *host, const char *service) {
    return echttp_raw_connect (host, service);
}

void echttp_listen (int fd, int mode, echttp_listener *listener, int premium) {
    echttp_raw_register (fd, mode, listener, premium);
}
 
void echttp_forget (int fd) {
    echttp_raw_forget (fd);
}
 
void echttp_background (echttp_listener *listener) {
    echttp_raw_background (listener);
}

static void echttp_listener_tls (int client, int mode) {

    mode = echttp_tls_ready (client, mode, echttp_received);
    if (mode < 0) {
        echttp_request *context = echttp_context[client];
        if (context->response) {
            context->status = 505;
            echttp_respond (client, 0, 0);
        }
        echttp_raw_close_client(client, "TLS failure");
        return;
    }
    echttp_raw_update (client, mode | 1);
}

static echttp_request *echttp_stacked = 0;

void echttp_escape (const char *s, char *d, int size) {
    echttp_encoding_escape (s, d, size);
}

const char *echttp_client (const char *method, const char *url) {

    int i, j;
    int socket;
    int client;
    char host[64];
    char service[16];
    char buffer[256];
    int start;
    int end;
    enum echttp_transport mode;

    if (!strncmp (url, "https://", 8)) {
        mode = ECHTTP_TLS;
        start = 8;
    } else if (!strncmp (url, "http://", 7)) {
        mode = ECHTTP_RAW;
        start = 7;
    } else return "unsupported";

    end = start + sizeof(host) - 1;
    for (i = start; i < end && url[i] && url[i] != ':' && url[i] != '/'; ++i) {
        host[i-start] = url[i];
    }
    host[i-start] = 0;
    service[0] = ':';
    if (url[i] == ':') {
        for (j = 1, ++i; j < 15 && url[i] && url[i] != '/'; ++i, ++j) {
            service[j] = url[i];
        }
        service[j] = 0;
    } else if (mode == ECHTTP_TLS) {
        service[1] = '4'; service[2] = '4'; service[3] = '3'; // Port 443.
        service[4] = 0;
        j = 0; // Mark that we use the default port.
    } else {
        service[1] = '8'; service[2] = '0'; // Port 80.
        service[3] = 0;
        j = 0; // Mark that we use the default port.
    }
    if (echttp_debug) printf ("Connecting to %s%s\n", host, service);
    socket = echttp_raw_connect(host, service+1); // Skip the ':'
    if (socket < 0) return "connection failed";

    switch (mode) {
        case ECHTTP_RAW:
             client = echttp_raw_manage (socket);
             break;
        case ECHTTP_TLS:
             client = echttp_raw_attach (socket, 3, echttp_listener_tls);
             switch (echttp_tls_attach (client, socket, host)) {
                 case -1:
                     // No need to call the response callback, because
                     // the query was not yet submitted.
                     echttp_raw_close_client (client, "TLS failed");
                     return "TLS failed";
                 case 0:
                 case 1:
                     echttp_raw_update (socket, 1); // Read only.
                     break;
                 case 2:
                     break;
             }
             break;
        default:
             return "invalid transport mode";
    }
    if (client < 0) return "no more client context";;

    echttp_newclient (client);
    echttp_stacked = echttp_current;
    echttp_current = echttp_context[client];
    echttp_current->mode = mode;

    snprintf (buffer, sizeof(buffer), "%s %s HTTP/1.1\r\n", method, url+i);
    echttp_send (client, buffer, strlen(buffer));

    snprintf (buffer, sizeof(buffer), "Host: %s%s\r\n", host, j?service:"");
    echttp_send (client, buffer, strlen(buffer));

    return 0;
}

int echttp_redirected (const char *method) {

    switch (echttp_current->status) {
        case 301:
        case 302:
        case 307:
        case 308: break; // Use requested method.

        case 303: method = "GET"; break;

        default: return echttp_current->status;
    }

    const char *redirect =
        echttp_catalog_get (&(echttp_current->in), "Location");
    if (redirect) {
        const char *error = echttp_client (method, redirect);
        if (!error) return 0;
    }
    return 500;
}

void echttp_submit (const char *data, int length,
                    echttp_response *response, void *origin) {

    echttp_current->response = response;
    echttp_current->origin = origin;

    echttp_send_data (echttp_current->client, data, length);
    echttp_current = echttp_stacked;
    echttp_stacked = 0;
}

