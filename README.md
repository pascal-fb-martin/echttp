## Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed for simple API and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)

After initializing the HTTP server, and then its own initialization, the application reacts to file descriptor events and HTTP requests as provided by the HTTP server.

## Installation
* Clone the repository.
* cd echttp
* make
* sudo make install (this installs files in the /usr/local tree)

Use the -lechttp option when building your application. For example:
```
cc -o httpserver httpserver.c -lechttp
```

## API
### Base HTTP Server
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
Define a route for processing the exact specified URI.
```
int echttp_route_match (const char *root, echttp_callback *call);
```
Defines a route for a parent URI and all its children.
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
void echttp_close (void);
```
Immediately close the HTTP server and all current HTTP connections.
### Static Pages Extension
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
