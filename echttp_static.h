/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 */
void echttp_static_map (const char *uri, const char *path);

const char *echttp_static_page (const char *action,
                                const char *uri,
                                const char *data, int length);

