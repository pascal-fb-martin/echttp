/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_static.h - An additional module to serve static page (files).
 */
void echttp_static_content_map (const char *extension, const char *content);

int  echttp_static_route (const char *uri, const char *path);

