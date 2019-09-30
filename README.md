## Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed for simple API and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)

After initializing the HTTP server, and then its own initialization, the application reacts to file descriptor events and HTTP requests as provided by the HTTP server.

## API
```
int echttp_open (int argc, const char **argv);
```
Initialize the HTTP server. The HTTP-specific arguments are removed from the argument list and the count of remaining arguments is returned.

```
typedef const char *echttp_callback (const char *method, const char *uri,
                                     const char *data, int length);
```
The profile for any HTTP request processing function.
```
int echttp_route_uri (const char *uri, ehttp_callback *call);
```
Define a route for processing the exact specified URI.
```
int echttp_route_match (const char *root, ehttp_callback *call);
```
Defines a route for a parent URI and all its children.
```
int echttp_route_static (const char *uri, const char *path);
```
Associate a parent URI with a local directory path: a child of the specified URI must match an existing file at the specified path.
```
const char *ehttp_attribute_get (const char *name); 
```
Retrieve the value of the specified HTTP attribute, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.
```
const char *ehttp_parameter_get  (const char *name);
```
Retrieve the value of the specified HTTP parameter, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.
```
void ehttp_attribute_set (const char *name, const char *value);
```
Set an attribute for the HTTP response. This function should be called from within an HTTP callback, while processing an HTTP request.
```
void ehttp_error (int code, const char *message);
```
The HTTP response will return the specified error instead of OK.
```
void ehttp_redirect (const char *url);
```
The HTTP response will return a redirect to the specified URL.
```
typedef void *ehttp_listener (int fd, int mode);
void ehttp_listen (int fd, int mode, ehttp_listener *listener);
```
Listen to the specified file descriptor (mode=0: don't listen, mode=1: read only, mode=2: write only, mode=3: read & write).

When the specified file descriptor is ready, the listener is called with the mode corresponding to the event.
```
echttp_close (void);
```
Immediately close the HTTP server and all current HTTP connections.
