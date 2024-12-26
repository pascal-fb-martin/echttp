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
 * -http-service=N: service name or port number to be used by the server.
 * -http-debug:     enable debug mode (verbose traces).
 */
const char *echttp_help (int level);

const char *echttp_option_match (const char *reference,
                                 const char *input, const char **value);

int echttp_option_present (const char *reference, const char *input);

void echttp_default (const char *arg);
int  echttp_open (int argc, const char **argv);
int  echttp_port (int ip);
int  echttp_dynamic_port (void);
void echttp_close (void);

typedef const char *echttp_callback (const char *method, const char *uri,
                                     const char *data, int length);

int echttp_route_uri (const char *uri, echttp_callback *call);
int echttp_route_match (const char *root, echttp_callback *call);
int echttp_route_find (const char *uri);

typedef void echttp_protect_callback (const char *method, const char *uri);

int echttp_protect (int route, echttp_protect_callback *call);

int echttp_asynchronous_route (int route, echttp_callback *callback);

const char *echttp_attribute_get (const char *name);
const char *echttp_parameter_get  (const char *name);
void        echttp_parameter_join (char *text, int size);

void echttp_attribute_set (const char *name, const char *value);
void echttp_error         (int code, const char *message);
void echttp_redirect      (const char *url);
void echttp_permanent_redirect (const char *url);

const char *echttp_reason (void);

void echttp_content_type_set  (const char *value);
void echttp_content_type_html (void);
void echttp_content_type_json (void);
void echttp_content_type_css  (void);

void echttp_transfer (int fd, int size);

int echttp_connect (const char *host, const char *service);

typedef void echttp_listener (int fd, int mode);
void echttp_listen (int fd, int mode, echttp_listener *listener, int premium);
void echttp_forget (int fd);
void echttp_background (echttp_listener *listener);
 
int echttp_isdebug (void);
int echttp_islocal (void);

void echttp_loop (void);

// Web client functions:

void echttp_escape (const char *s, char *d, int size);

typedef void echttp_response (void *origin, int status, char *data, int length);

const char *echttp_client (const char *method, const char *url);
void echttp_asynchronous  (echttp_response *asynchronous);
void echttp_submit        (const char *data, int length,
                           echttp_response *response, void *origin);

int echttp_redirected (const char *method);
#endif

