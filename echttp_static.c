/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * const char *echttp_static_page (const char *action,
 *                                 const char *uri,
 *                                 const char *data, int length);
 *
 *    Execute an HTTP request for a static page (i.e. file).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "echttp.h"

const char *echttp_static_page (const char *action,
                                const char *uri,
                                const char *data, int length) {
    return 0;
}

