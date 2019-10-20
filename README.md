# Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed for simple API and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)

After initializing the HTTP server, and then its own initialization, the application reacts to file descriptor events and HTTP requests as provided by the HTTP server.

# Installation
* Clone the repository.
* cd echttp
* make
* sudo make install (this installs files in the /usr/local tree)

Use the -lechttp option when building your application. For example:
```
cc -o httpserver httpserver.c -lechttp
```
# Example
Below is an example of a small, but fully functional web server written using echttp. This server returns pages from the current directory as well as an embedded welcome message.
```
#include <stdio.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_static.h"

static const char *welcome (const char *method, const char *uri,
                            const char *data, int length) {
    static char buffer[1024];
    const char *host = echttp_attribute_get("Host");
    const char *who = echttp_parameter_get("who");
    if (host == 0) host = "(unknown)";
    if (who == 0) who = "Anonymous";
    echttp_content_type_html ();
    snprintf (buffer, sizeof(buffer),
              "<e>You are welcome on <b>%s</b>, dear <i>%s</i>!</e>", host, who);
    return buffer;
}

int main (int argc, const char **argv) {
    if (echttp_open (argc, argv) >= 0) {
        echttp_route_uri ("/welcome", welcome);
        echttp_static_route ("/static", getcwd(0, 0));
        echttp_loop();
    }
    return 1;
}
```
# API
## Base HTTP Server
The application must include echttp.h as a prerequisit to using the echttp API.

The echttp primitives are:
```
int echttp_open (int argc, const char **argv);
```
Initialize the HTTP server. The HTTP-specific arguments are removed from the argument list and the count of remaining arguments is returned.

The arguments consumed by echttp_open are:
* -http-port=_port_ (Port number or service name to listen to; default is "http", i.e. port 80.)
* -http-debug (If present, expect to see a lot of debug traces.)

```
typedef const char *echttp_callback (const char *method, const char *uri,
                                     const char *data, int length);
```
The profile for any HTTP request processing function.
```
int echttp_route_uri (const char *uri, echttp_callback *call);
```
Define a route for processing the exact specified URI. Return the route descriptor or -1 on failure.
```
int echttp_route_match (const char *root, echttp_callback *call);
```
Defines a route for a parent URI and all its children. Return the route descriptor or -1 on failure.
```
typedef void echttp_protect_callback (const char *method, const char *uri);
int echttp_protect (int route, echttp_protect_callback *call);
```
Declare a protect callback for the specified route. The route descriptor is the value returned by echttp_route_uri() or echttp_route_match(). A protect callback is called before the route's callback and may change the HTTP status to not OK and set the response's attributes; in that case the route's callback function is not called.

This is meant to facilitate the implementation of access control extensions, this is not an access control method on its own.
```
const char *echttp_attribute_get (const char *name); 
```
Retrieve the value of the specified HTTP attribute, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.
```
const char *echttp_parameter_get  (const char *name);
```
Retrieve the value of the specified HTTP parameter, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.

Parameter names are case insensitive: __NaMe__ is the same as __name__.
```
void echttp_attribute_set (const char *name, const char *value);
```
Set an attribute for the HTTP response. This function should be called from within an HTTP callback, while processing an HTTP request.
```
void echttp_content_type_set (const char *value);
```
A shorthand for setting the Content-Type attribute.
```
void echttp_content_type_html (void);
```
A shorthand for setting the Content-Type attribute to HTML.
```
void echttp_content_type_json (void);
```
A shorthand for setting the Content-Type attribute to JSON.
```
void echttp_content_type_css (void);
```
A shorthand for setting the Content-Type attribute to CSS.
```
void echttp_error (int code, const char *message);
```
The HTTP response will return the specified error instead of OK.
```
void echttp_redirect (const char *url);
```
The HTTP response will return a redirect to the specified URL.
```
typedef void *echttp_listener (int fd, int mode);
void echttp_listen (int fd, int mode, echttp_listener *listener, int premium);
```
Listen to the specified file descriptor (mode=0: don't listen, mode=1: read only, mode=2: write only, mode=3: read & write).

When the specified file descriptor is ready, the listener is called with the mode corresponding to the event.

The premium option, if used, causes this file descriptor to be processed before any HTTP client and other listener. The premium option is meant for a high priority I/O.
```
void echttp_background (echttp_listener *listener);
```
Call the specified listener before waiting for I/O. This listener is called with fd 0 and mode 0; it must not block on I/O itself, but it is allowed to call echttp_listen(), changing the list of I/O to listen to on the next cycle. Expect this listener to be called once a second at least, if not a lot more.

There is only one background listener.
```
int echttp_isdebug (void);
```
Return true if the HTTP debug option was selected. Only used for debug or troubleshooting.
```
void echttp_loop (void);
```
Run the HTTP server. This function typically never return. If it does, all HTTP server resources have been closed and deallocated. The most reasonable thing to do at this point is to exit.
```
void echttp_close (void);
```
Immediately close the HTTP server and all current HTTP connections.
## Static Pages Extension
The echttp library can serve local files, from several separate locations if needed. This capacity is a separate extension and requires to include echttp_static.h.
The static page extension primitives are:
```
void echttp_static_content_map (const char *extension, const char *content);
```
Define the content type associated with a specific file extension. The content type is implicitely defined for the following file extensions: html, htm, json, jsn, css.
```
int  echttp_static_route (const char *uri, const char *path);
```
Associate a parent URI with a local directory path: a child of the specified URI will match an existing file at the specified path (including the URI's child relative path).

For example if one defines a static route from /static to /home/doe/public, the URI /static/fancy/interface.html will route to /home/doe/public/fancy/interface.html.

As soon as a static route has been declared, the extension takes over the root URI "/". If the root URI is requested, the extension seaches for file index.html in every path provided and returns the content of the first one found.

This function returns the route descriptor or -1 on failure. This route descriptor can be used to protect the whole path.

