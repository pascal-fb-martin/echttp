## Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed for simple API and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)
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
echttp_close (void);
```
Immediately close the HTTP server and all current HTTP connections.
