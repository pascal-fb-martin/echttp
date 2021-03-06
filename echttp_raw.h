/* echttp - Embedded HTTP server.
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 */
#include "echttp.h"

typedef int  echttp_raw_acceptor (int client);
typedef int  echttp_raw_receiver (int client, char *data, int length);
typedef void echttp_raw_terminator (int client, const char *reason);

int  echttp_raw_open (const char *service, int debug, int ttl);

int  echttp_raw_capacity (void);
int  echttp_raw_server_port (int ip);

void echttp_raw_send (int client, const char *data, int length);
void echttp_raw_transfer (int client, int fd, int length);

void echttp_raw_background (echttp_listener *listener);

void echttp_raw_loop (echttp_raw_acceptor   *acceptor,
                      echttp_raw_receiver   *received,
                      echttp_raw_terminator *terminate);

int  echttp_raw_is_local (int client);

int echttp_raw_connect (const char *host, const char *service);

int echttp_raw_manage   (int s);
int echttp_raw_attach   (int fd, int mode, echttp_listener *listener);
int echttp_raw_register (int fd, int mode,
                         echttp_listener *listener, int premium);

int echttp_raw_update   (int client, int mode);

void echttp_raw_close_client (int i, const char *reason);
void echttp_raw_forget (int fd);
void echttp_raw_close (void);

