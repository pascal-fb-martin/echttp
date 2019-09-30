/* echttp - Embedded HTTP server.
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 */
typedef int echttp_raw_callback (int client, char *data, int length);

int  echttp_raw_open (const char *service, int debug);

void echttp_raw_send (int client, const char *data, int length, int hangup);

void echttp_raw_loop (echttp_raw_callback *received);

void echttp_raw_close (void);

