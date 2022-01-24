/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.

 * This module implements the HTTP encoding rules.
 */

char *echttp_encoding_escape (const char *s, char *d, int size);

char *echttp_encoding_unescape (char *data);

