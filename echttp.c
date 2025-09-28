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
 * and should not block on I/O itself. It will not be called more
 * frequently than every second, but this is not a precise timer.
 *
 * void echttp_fastscan (echttp_listener *listener, int period);
 *
 * Call this listener periodically. The period is expressed in millisecond
 * and must be within the interval ]0, 1000[. This can be used concurrently
 * with the background mechanism. It is more expensive in term of overhead,
 * but it is also more accurate in term of timing. The listener will always
 * be called with with fd 0 and mode 0.
 *
 * void echttp_loop (void);
 * 
 * Enter the HTTP server main loop. The HTTP server may call any callback
 * or listener function, in any order.
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
#include <unistd.h>

#include "echttp.h"
#include "echttp_raw.h"
#include "echttp_tls.h"
#include "echttp_catalog.h"
#include "echttp_encoding.h"

static const char *echttp_service = "http";
static int         echttp_debug = 0;


#define ECHTTP_STATE_IDLE    0
#define ECHTTP_STATE_CONTENT 1
#define ECHTTP_STATE_ERROR   2

#define ECHTTP_TRANSFER_IDLE 0
#define ECHTTP_TRANSFER_IN   1
#define ECHTTP_TRANSFER_OUT  2

enum echttp_transport {ECHTTP_RAW = 0, ECHTTP_TLS = 1};

typedef struct echttp_queue_data *echttp_queue;

struct echttp_queue_data {
    echttp_queue next;
    const char *data;
    int length;
};

typedef struct {
    enum echttp_transport mode;
    short state;
    short protected;
    int route;
    int client;
    int contentlength;
    int contentlengthout;
    char method[64];
    char uri[512];
    char *content;
    echttp_catalog in;
    echttp_catalog out;
    echttp_catalog params;

    int status;
    const char *reason;

    echttp_queue next;
    echttp_queue last;
    int queued;

    struct {
        int state; // IDLE, IN or OUT (see ECHTTP_TRANSFER_ constants)
        int fd;
        int size;
    } transfer;

    echttp_response *response;
    echttp_response *asynchronous;
    void *origin;

} echttp_request;

static echttp_request **echttp_context = 0;
static int              echttp_context_count = 0;
static echttp_request *echttp_current = 0;

static int echttp_dynamic_flag = 0;

#define ECHTTP_MATCH_EXACT   1
#define ECHTTP_MATCH_PARENT  2
#define ECHTTP_MATCH_ANY     (ECHTTP_MATCH_EXACT|ECHTTP_MATCH_PARENT)

typedef struct {
    const char *uri;
    echttp_callback *call;
    echttp_callback *asynchronous;
    echttp_protect_callback *protect;
    unsigned int signature;
    int match;
    int next;
} echttp_route;

#define ECHTTP_MAX_ROUTES 512
static struct {
    int count;
    int index[ECHTTP_HASH];
    echttp_route item[ECHTTP_MAX_ROUTES];
    echttp_protect_callback *protect;
} echttp_routing;


static void echttp_transfer_reset (echttp_request *context) {
   context->transfer.state = ECHTTP_TRANSFER_IDLE;
   context->transfer.fd = -1;
   context->transfer.size = 0;
}

static void echttp_transfer_cancel (echttp_request *context) {
   if ((context->transfer.fd >= 0) &&
       (context->transfer.state != ECHTTP_TRANSFER_IDLE)) {
       close (context->transfer.fd);
   }
   echttp_transfer_reset (context);
}

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

static void echttp_send_content (int client, const char *data, int length) {

    echttp_request *context = echttp_context[client];
    static const char eol[] = "\r\n";
    int i;
    char buffer[256];
    int transfer_size = 0;

    if ((context->transfer.size > 0) &&
        (context->transfer.state == ECHTTP_TRANSFER_OUT))
        transfer_size = context->transfer.size;

    int size = snprintf (buffer, sizeof(buffer)-1,
                         "Content-Length: %d\r\n",
                         length + context->queued + transfer_size);
    echttp_send (client, buffer, size);

    for (i = 1; i <= context->out.count; ++i) {
        if (context->out.item[i].name == 0) continue;
        size = snprintf (buffer, sizeof(buffer)-1, "%s: %s\r\n",
                         context->out.item[i].name,
                         (char *)(context->out.item[i].value));
        echttp_send (client, buffer, size);
    }
    echttp_send (client, eol, sizeof(eol)-1);
    if (length > 0) {
       echttp_send (client, data, length);
    }

    // Data queued can now be sent. Count on echttp_raw to absorb
    // any burst size (with its own buffers).
    echttp_queue cursor = context->next;
    while (cursor) {
        echttp_send (client, cursor->data, cursor->length);
        echttp_queue next = cursor->next;
        free ((void*)(cursor->data));
        free (cursor);
        cursor = next;
    }
    context->next = context->last = 0;
    context->queued = 0;

    if (transfer_size > 0) {
        // This transfer must be submitted to the raw layer only after
        // all the preamble was submitted. Otherwise the raw layer may
        // start the file transfer before the HTTP preamble was sent..
        //
        // Once this has been submitted to the raw layer, forget about it:
        // the raw layer has become responsible for closing the file.
        //
        echttp_raw_transfer (client,
                             context->transfer.fd,
                             context->transfer.size);
        echttp_transfer_reset (context); // Forget.
    }
}

static void echttp_send_error (int client, int status, const char *text) {

    // Note: an error reported by HTTP is not a protocol error that requires
    // breaking the connection. (Hint: the Debian apt client uses repeated
    // HTTP requests to check which files are present.)
    //
    static const char errorformat[] =
        "HTTP/1.1 %d %s\r\n"
        "Content-Length: 0\r\n\r\n";

    echttp_request *context = echttp_context[client];
    char buffer[1024];
    int length = snprintf (buffer, sizeof(buffer), errorformat, status, text);
    echttp_send (client, buffer, length);
    echttp_transfer_cancel (context);
}

static void echttp_unknown (int client) {
    echttp_send_error (client, 404, "Not found");
}

static void echttp_invalid (int client, const char *text) {
    echttp_send_error (client, 406, text);
}

static int echttp_has_error (int client) {
    return ((echttp_context[client]->status / 100) > 3);
}

static int echttp_execute_protect (int route, int client,
                                   const char *action, const char *uri) {

    echttp_request *context = echttp_context[client];

    if (context->protected) return 1; // No need to do this twice.

    context->status = 200;
    context->reason = "OK";
    echttp_catalog_reset(&(context->out));
    echttp_transfer_reset (context);

    if (echttp_routing.protect) {
        echttp_routing.protect (action, uri);
    }
    if (context->status == 200) {
        if (echttp_routing.item[route].protect) {
            echttp_routing.item[route].protect (action, uri);
        }
    }
    if (context->status == 204) {
        // 204 is not really an HTTP error, but at the protect phase
        // this is a polite way to say "I will not process this".
        //
        echttp_send_error (client, 204, context->reason);
        return 0; // Skip processing.
    }

    // Any other 2xx status is OK, but an HTTP error cancels the request.
    //
    if (echttp_has_error (client)) {
        echttp_send_error (client, context->status, context->reason);
        return 0; // Failed.
    }
    context->protected = 1;
    return 1; // Passed.
}

static void echttp_execute_async (int route, int client,
                                  const char *action, const char *uri,
                                  const char *data, int length) {

    echttp_request *context = echttp_context[client];

    echttp_current = context;
    if (! echttp_execute_protect (route, client, action, uri)) {
        echttp_current = 0;
        return;
    }
    echttp_routing.item[context->route].asynchronous (action, uri, data, length);
    echttp_current = 0;

    if ((context->status / 100) == 3) {
        // This is a redirect and this request will not be processed further.
        // The client will reissue a new request using the redirection URL and
        // this connection will be closed. Better to send the HTTP status now
        // and ignore any of the content data that will not be used anyway.
        //
        char buffer[256];
        snprintf (buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n",
                 context->status, context->reason);
        echttp_send (client, buffer, strlen(buffer));
        echttp_send_content (client, 0, 0);
        context->state = ECHTTP_STATE_ERROR; // Ignore further data.
    }

    if (echttp_has_error (client)) {
        // This is bad, as the request is nowhere complete.
        // Send a status now and cancel the whole request.
        echttp_send_error (client, context->status, context->reason);
    }
    // Do not send anything when there is no error or redirection, since
    // we are still waiting for more content data.
}

static void echttp_date (void) {
    // See RFC 2616, section 14.18.
    static char datestring[80];
    time_t now = time(0);
    int result = strftime (datestring, sizeof(datestring),
                           "%a, %d %b %Y %H:%M:%S %Z", gmtime (&now));
    if (result > 0) echttp_attribute_set ("Date", datestring);
}

static void echttp_execute (int route, int client,
                           const char *action, const char *uri,
                           const char *data, int length) {

    echttp_request *context = echttp_context[client];

    // Do not rely on echttp_current internally: the application is allowed
    // to submit a client request in reaction to the received request,
    // so the current context might have been replaced.
    //
    const char *connection = echttp_catalog_get (&(context->in), "Connection");
    int keep = (connection && (strcmp(connection, "keep-alive") == 0));

    echttp_current = context;
    if (! echttp_execute_protect (route, client, action, uri)) {
        echttp_current = 0;
        return;
    }
    context->contentlengthout = 0;
    data = echttp_routing.item[route].call (action, uri, data, length);
    echttp_date ();
    echttp_current = 0;

    if (echttp_has_error (client)) {
        echttp_send_error (client, context->status, context->reason);
        return;
    }
    if (! data)
        length = 0;
    else if (context->contentlengthout > 0)
        length = context->contentlengthout;
    else
        length = strlen(data);

    char buffer[256];
    snprintf (buffer, sizeof(buffer), "HTTP/1.1 %d %s\r\n",
             context->status, context->reason);
    echttp_send (client, buffer, strlen(buffer));

    if (keep) {
        static const char text[] = "Connection: keep-alive\r\n";
        echttp_send (client, text, sizeof(text)-1);
    }
    echttp_send_content (client, data, length);
}

static int echttp_route_add (const char *uri, echttp_callback *call, int match) {

    // Try reusing a discarded slot first. This is not the most efficient
    // method (make a free list instead?) but adding a route is rare..
    // If no discarded slot is found, then "i" will automatically index
    // a new entry.
    int i;
    for (i = 1; i <= echttp_routing.count; ++i) {
        if (!echttp_routing.item[i].uri) break;
    }
    if (i >= ECHTTP_MAX_ROUTES) {
        fprintf (stderr, "Too many routes.\n");
        return -1;
    }
    if (i > echttp_routing.count) echttp_routing.count = i; // New slot.

    unsigned int signature = echttp_hash_signature (uri);
    int index = signature % ECHTTP_HASH;

    echttp_routing.item[i].uri = uri;
    echttp_routing.item[i].call = call;
    echttp_routing.item[i].protect = 0;
    echttp_routing.item[i].asynchronous = 0;
    echttp_routing.item[i].match = match;
    echttp_routing.item[i].signature = signature;
    echttp_routing.item[i].next = echttp_routing.index[index];
    echttp_routing.index[index] = i;
    return i;
}

static int echttp_route_search (const char *uri, int match) {
   int i;
   unsigned int signature = echttp_hash_signature (uri);
   int index = signature % ECHTTP_HASH;

   static char *toascii[] = {"(invalid)", "exact", "parent", "any"};

   if (echttp_debug)
       printf ("Searching route for %s (match %s)\n", uri, toascii[match]);
   for (i = echttp_routing.index[index];
        i > 0; i = echttp_routing.item[i].next) {
       if ((echttp_routing.item[i].match & match) == 0) continue;
       if (echttp_debug)
           printf ("Matching with %s (%s entry)\n",
                   echttp_routing.item[i].uri,
                   toascii[echttp_routing.item[i].match]);
       if ((echttp_routing.item[i].signature == signature) &&
           strcmp (echttp_routing.item[i].uri, uri) == 0) return i;
   }
   return -1;
}

int echttp_route_find (const char *uri) {
    return echttp_route_search (uri, ECHTTP_MATCH_ANY);
}

static void echttp_respond_async (int client, char *data, int length) {

   // Do not rely on echttp_current internally: this is processing a response
   // and there is no current request processing.
   //
   echttp_request *context = echttp_context[client];

   if (context->asynchronous) {
       echttp_current = context;
       context->asynchronous (context->origin, context->status, data, length);
       echttp_current = 0;
       context->asynchronous = 0;
   }
}

static void echttp_respond (int client, char *data, int length) {

   // Do not rely on echttp_current internally: this is processing a response
   // and there is no current request processing.
   //
   echttp_request *context = echttp_context[client];

   if (context->response) {
       echttp_current = context;
       context->response (context->origin, context->status, data, length);
       echttp_current = 0;
       context->response = 0;
   }
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
       context->next = context->last = 0;
   }
   context->state = ECHTTP_STATE_IDLE;
   context->mode = ECHTTP_RAW;
   echttp_transfer_reset (context);
   context->response = 0;
   context->asynchronous = 0;
   context->origin = 0;
   context->route = 0;
   echttp_catalog_reset(&(context->in));
   echttp_catalog_reset(&(context->out));
   context->queued = 0;
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
       echttp_transfer_cancel(context); // Done with data transfer, if any.
       if (context->response) {
           context->status = 505;
           echttp_respond (client, 0, 0);
       }
       return 0; // The connection will be closed by echttp_raw.
   }

   // If any error has been detected, ignore all further data until the
   // connection is closed.
   if (context->state == ECHTTP_STATE_ERROR) return length;

   if (echttp_debug)
       printf ("Received HTTP %s (%d bytes)\n",
               echttp_context[client]->response?"response":"request", length);
   data[length] = 0;

   // If there was content left to receive, accumulate it in the echttp_raw
   // buffer (synchronous mode), or write it to the file provided (asynchronous
   // mode).
   // When we received it all, execute the HTTP request. Otherwise wait for more.
   //
   if (context->state == ECHTTP_STATE_CONTENT) {
       if (context->transfer.state == ECHTTP_TRANSFER_IN) { // Asynchronous.
           int size = length;
           if (context->transfer.size < length) size = context->transfer.size;
           size = write (context->transfer.fd, data, size);
           if (size <= 0) {
               context->state = ECHTTP_STATE_ERROR;
               echttp_transfer_cancel(context);
               return length; // Discard all received data.
           }
           context->transfer.size -= size;
           if (context->transfer.size <= 0) {
               echttp_transfer_cancel(context); // Done with that transfer.
               if (context->response) {
                   // Client connections are not reused: handle the response
                   // and close.
                   echttp_respond (client, 0, 0);
                   echttp_raw_close_client(context->client, "end of response");
                   return 0; // Connection was closed, nothing more to do.
               }
               // Server connections are kept open: the client may submit
               // a subsequent request.
               context->state = ECHTTP_STATE_IDLE;
               echttp_execute (context->route, client,
                               context->method, context->uri, 0, 0);
           }
           return size;
       }

       // Synchronous.
       if (context->contentlength > length) return 0; // Wait for more.
       if (context->response) {
           // Client connections are not reused: handle the response
           // and close.
           echttp_respond (client, data, context->contentlength);
           echttp_raw_close_client(context->client, "end of response");
           return 0; // Connection was closed, nothing more to do.
       }
       // Server connections are kept open: the client may submit
       // a subsequent request.
       echttp_execute (context->route, client,
                       context->method, context->uri,
                       data, context->contentlength);
       consumed = context->contentlength;
       data += context->contentlength;
       context->state = ECHTTP_STATE_IDLE;
   }

   // We are waiting for a new HTTP PDU.
   // The HTTP standard allows sending requests back to back, however
   // echttp cannot send a new response before the previous has been sent
   // when the transfer mechanism is being used. The problem is that there
   // is no mechanism to sequence multiple transfers one after the other,
   // combined with buffer data.
   // Here we loop _until_ a full HTTP request has been processed, at which
   // time the code breaks free.
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
               echttp_invalid (client, "Invalid Request Line");
               return length; // Consume everything, since we are closing.
           }
           wordcount = echttp_split (request[1], "?", rawuri, 4);
           char *method = echttp_encoding_unescape(request[0]);
           char *uri = echttp_encoding_unescape(rawuri[0]);

           if (strstr (uri, "..")) {
               // There is no legitimate reason to use ".." in any URL, even
               // this is not a file URL.
               // (The same protection is already implemented in echttp_static
               // and then in a few other places. That it was needed in more
               // than one place is what motivated this low-level check: too
               // much risk of having the check missing in some places.
               echttp_raw_close_client(context->client, "path traversal");
               return 0; // Connection was closed, nothing more to do.
           }

           if ((method == 0)  || (uri == 0)) {
               echttp_invalid (client, "Invalid request format");
               return length; // Consume everything, since we are closing.
           }
           snprintf (context->method, sizeof(context->method), method);
           snprintf (context->uri, sizeof(context->uri), uri);

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
                           echttp_invalid (client, "Invalid Parameter Syntax");
                           return length; // Consume everything, invalid.
                       }
                       echttp_catalog_set (&(context->params), name, value);
                   }
               }
           }

           // Search for a uri mapping: try any match first, then for a parent.
           //
           i = echttp_route_search (context->uri, ECHTTP_MATCH_ANY);
           if (i <= 0) {
               char *uri = strdup(context->uri);
               char *sep = strrchr (uri+1, '/');
               while (sep) {
                   *sep = 0;
                   i = echttp_route_search (uri, ECHTTP_MATCH_PARENT);
                   if (i > 0) break;
                   sep = strrchr (uri+1, '/');
               }
               if (i <= 0) {
                   i = echttp_route_search ("/", ECHTTP_MATCH_PARENT);
               }
               free(uri);
               if (i <= 0) {
                   echttp_unknown (client);
                   return length; // Consume everything, since we are closing.
               }
           }
           context->route = i;
       }

       // Decode the header attributes after the request/status line.
       //
       echttp_catalog_reset(&(context->in));
       for (i = 1; i < linecount; ++i) {
           char *param[4];
           if (echttp_split (line[i], ": ", param, 4) >= 2) {
               echttp_catalog_set (&(context->in), param[0], param[1]);
           }
       }

       // At this point all the HTTP request and header lines were decoded
       // and we are ready to act on that new request.
       //
       context->protected = 0; // The protect callbacks were not called yet.

       // Retrieve the content data already received.
       //
       const char *field = echttp_catalog_get(&(context->in), "Content-Length");
       context->contentlength = 0;
       context->content = endreq;
       if (field) {
          int available = (int)(enddata - endreq);
          context->contentlength = atoi(field);
          if (context->contentlength < 0) context->contentlength = 0;
          if (context->contentlength > available) {
              if (echttp_debug) printf("HTTP: waiting for end of content.\n");
              context->state = ECHTTP_STATE_CONTENT;
              consumed += ((int) (endreq - data)); // Consumed the header.

              // Asynchronous request (client side).
              if (context->asynchronous) {
                  if (echttp_debug) printf("HTTP: asynchronous response.\n");
                  echttp_respond_async (client, context->content, available);
                  if (context->state == ECHTTP_STATE_ERROR) return length;
                  if (context->transfer.state == ECHTTP_TRANSFER_IN)
                      consumed += available; // Consumed the received content.

              // Asynchronous endpoint (server side).
              } else if ((context->route > 0) &&
                  echttp_routing.item[context->route].asynchronous) {
                  if (echttp_debug) printf("HTTP: asynchronous request.\n");
                  echttp_execute_async (context->route, client,
                                        context->method, context->uri,
                                        context->content, available);
                  if (context->state == ECHTTP_STATE_ERROR) return length;
                  if (context->transfer.state == ECHTTP_TRANSFER_IN)
                      consumed += available; // Consumed the received content.
              }
              return consumed; // Wait for more.
          }
          // All expected data was received.
          consumed += ((int) (endreq - data) + context->contentlength);
          data = endreq + context->contentlength;
       } else {
           // Maybe this is (short) chunked data..
           field = echttp_catalog_get(&(context->in), "Transfer-Encoding");
           if (field) {
               if (strcmp (field, "chunked")) {
                   echttp_raw_close_client(context->client,
                                           "unsupported transfer encoding");
                   return length; // Consume everything, invalid.
               }
               // Time to panic: the length is not known yet, as it will
               // be provided in chunks. Did we receive it all?
               int extracted = 0;
               char *decode = endreq;
               char *cursor = endreq;
               for (;;) {
                   while ((*decode) && (*decode <= ' ')) decode += 1;
                   if (decode >= enddata) {
                       echttp_raw_close_client(context->client,
                                               "incomplete chunked data");
                       return 0; // Nothing more to do with this connection.
                   }
                   int size = strtol (decode, &decode, 16);
                   while (*decode != '\n') decode += 1;
                   decode += 1; // Skip LF.
                   if (size == 0) break; // Last chunk.

                   // Eliminate the chunking by moving the data.
                   memmove (cursor, decode, size);
                   decode += size; // Skip the data.
                   cursor += size;
                   extracted += size;
               }
               // We received it all: accept that easy case for now.
               *cursor = 0;
               context->contentlength = extracted;
               consumed = (int) (decode - data);
               data = decode;
           } else {
               // No data?
              consumed += ((int) (endreq - data)); // Consumed the header.
              data = endreq;
           }
       }
       if (!context->contentlength) context->content = 0;

       if (context->response) {
           echttp_respond (client, context->content, context->contentlength);
           echttp_raw_close_client(context->client, "end of response");
           return 0; // Nothing more to do with this connection.
       }
       echttp_execute (context->route, client,
                      context->method, context->uri,
                      context->content, context->contentlength);
       // Avoid processing a subsequent request before the response has been
       // sent. Otherwise there could be a problem with concurrent transfers.
       break;
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
    echttp_queue cursor = echttp_context[client]->next;
    while (cursor) {
        echttp_queue next = cursor->next;
        free ((void*)(cursor->data));
        free (cursor);
        cursor = next;
    }
    echttp_context[client]->next = echttp_context[client]->last = 0;
    echttp_context[client]->queued = 0;
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
    return echttp_route_add (uri, call, ECHTTP_MATCH_EXACT);
}

int echttp_route_match (const char *root, echttp_callback *call) {
    return echttp_route_add (root, call, ECHTTP_MATCH_PARENT);
}

void echttp_route_remove (const char *uri) {

    int i = echttp_route_find (uri);
    if (i <= 0) return;

    // Remove the route from the hash list.
    // This may take a loop since this is a single link list,
    // but the remove operation is considered rare.
    // Plus this is a collision list, which is short almost all the time.
    //
    unsigned int signature = echttp_hash_signature (uri);
    int index = signature % ECHTTP_HASH;
    if (echttp_routing.index[index] == i)
       echttp_routing.index[index] = echttp_routing.item[i].next;
    else {
       int j;
       for (j = echttp_routing.index[index];
            j > 0; j = echttp_routing.item[j].next) {
            if (echttp_routing.item[j].next == i) {
               echttp_routing.item[j].next = echttp_routing.item[i].next;
               break;
            }
       }
    }

    echttp_routing.item[i].uri = 0; // Garbage collection is up to the caller.
}

int echttp_protect (int route, echttp_protect_callback *call) {
    if (route == 0)
        echttp_routing.protect = call;
    else {
        if (route < 0 || route > echttp_routing.count) return -1;
        if (!echttp_routing.item[route].uri) return -1;
        echttp_routing.item[route].protect = call;
    }
    return route;
}

int echttp_asynchronous_route (int route, echttp_callback *callback) {
    if (route <= 0 || route > echttp_routing.count) return -1;
    if (!echttp_routing.item[route].uri) return -1;
    echttp_routing.item[route].asynchronous = callback;
    return route;
}

const char *echttp_attribute_get (const char *name) {
    if (! echttp_current) return 0;
    return echttp_catalog_get (&(echttp_current->in), name);
}

const char *echttp_parameter_get (const char *name) {
    if (! echttp_current) return 0;
    return echttp_catalog_get (&(echttp_current->params), name);
}

void echttp_parameter_join (char *text, int size) {
    if (! echttp_current) return;
    echttp_catalog_join (&(echttp_current->params), "&", text, size);
}

void echttp_attribute_set (const char *name, const char *value) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out), name, value);
}

void echttp_content_type_set (const char *value) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out), "Content-Type", value);
}

void echttp_content_type_text (void) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out),
                        "Content-Type", "text/plain");
}

void echttp_content_type_json (void) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out),
                        "Content-Type", "application/json");
}

void echttp_content_type_html (void) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out), "Content-Type", "text/html");
}

void echttp_content_type_css (void) {
    if (! echttp_current) return;
    echttp_catalog_set (&(echttp_current->out), "Content-Type", "text/css");
}

void echttp_content_length (int length) {
    if (! echttp_current) return;
    echttp_current->contentlengthout = length;
}

void echttp_content_queue (const char *data, int length) {
    if (! echttp_current) return;
    echttp_queue item = malloc (sizeof(struct echttp_queue_data));
    item->next = 0;
    item->data = data;
    item->length = length;
    if (echttp_current->last)
       echttp_current->last->next = item;
    else
       echttp_current->next = item;
    echttp_current->last = item;
    echttp_current->queued += length;
}

void echttp_transfer (int fd, int size) {

    if (!echttp_current) return;
    if (echttp_current->transfer.state != ECHTTP_TRANSFER_IDLE) return;

    echttp_current->transfer.state = ECHTTP_TRANSFER_OUT; // Default.

    if (echttp_current->state == ECHTTP_STATE_CONTENT) {
       if (echttp_current->asynchronous ||
           (echttp_current->route &&
            echttp_routing.item[echttp_current->route].asynchronous)) {
           echttp_current->transfer.state = ECHTTP_TRANSFER_IN;
       }
    }
    echttp_current->transfer.fd = fd;
    echttp_current->transfer.size = size;
}

void echttp_error (int code, const char *message) {
    if (! echttp_current) return;
    echttp_current->status = code;
    echttp_current->reason = message;
}

const char *echttp_reason (void) {
    if (! echttp_current) return 0;
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
    if (! echttp_current) return 0;
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

void echttp_fastscan (echttp_listener *listener, int period) {
    echttp_raw_fastscan (listener, period);
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

void echttp_asynchronous (echttp_response *asynchronous) {
    if (echttp_current) {
        echttp_current->asynchronous = asynchronous;
    }
}

static void echttp_copy_attributes (echttp_request *src, echttp_request *dst) {
    int i;
    for (i = 1; i <= src->out.count; ++i) {
        if (src->out.item[i].name == 0) continue;
        echttp_catalog_set (&(dst->out),
                            src->out.item[i].name, src->out.item[i].value);
    }
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
        // With this redirection, the pending request is complete.
        // We need to open a new connection to the redirected location.
        // This new connection does not stack on top of the original
        // request: is replaces it, with the same HTTP header attributes.
        echttp_request *redirected = echttp_current;
        echttp_request *stacked = echttp_stacked;
        const char *error = echttp_client (method, redirect);
        echttp_stacked = stacked;
        if (error) return 500;
        echttp_copy_attributes (redirected, echttp_current);
        return 0;
    }
    return 500;
}

void echttp_submit (const char *data, int length,
                    echttp_response *response, void *origin) {

    echttp_current->response = response;
    echttp_current->origin = origin;

    echttp_send_content (echttp_current->client, data, length);
    echttp_current = echttp_stacked;
    echttp_stacked = 0;
}

