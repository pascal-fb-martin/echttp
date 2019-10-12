/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * applications.
 *
 * The server may listen to multiple simultaneous requests (i.e. TCP
 * connections), but each HTTP request, once received in full, is blocking
 * (i.e. no other HTTP request is processed until the callback returns).
 */

#ifndef ECHTTP_H__INCLUDED
#define ECHTTP_H__INCLUDED

/*
 * The open function understands the following options:
 * -http=N: port number or name to be used by the server.
 */
int  echttp_open (int argc, const char **argv);
void echttp_close (void);

typedef const char *echttp_callback (const char *method, const char *uri,
                                     const char *data, int length);

int echttp_route_uri (const char *uri, echttp_callback *call);
int echttp_route_match (const char *root, echttp_callback *call);

const char *echttp_attribute_get (const char *name);
const char *echttp_parameter_get  (const char *name);

void echttp_attribute_set (const char *name, const char *value);
void echttp_error         (int code, const char *message);
void echttp_redirect      (const char *url);

void echttp_content_type_set (const char *value);

typedef void echttp_listener (int fd, int mode);
void echttp_listen (int fd, int mode, echttp_listener *listener, int premium);
void echttp_background (echttp_listener *listener);
 
int echttp_isdebug (void);

void echttp_loop (void);

#endif

