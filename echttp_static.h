/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_static.h - An additional module to serve static page (files).
 */

// These two initialization functions are optional.
void echttp_static_default (const char *arg);
void echttp_static_initialize (int argc, const char *argv[]);

typedef int echttp_not_found_handler (const char *path);
echttp_not_found_handler *echttp_static_on_not_found (echttp_not_found_handler *handler);

void echttp_static_content_map (const char *extension, const char *content);

int  echttp_static_route (const char *uri, const char *path);

