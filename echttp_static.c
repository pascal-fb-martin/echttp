/* echttp - Embedded HTTP server.
 *
 * Copyright 2019, Pascal Martin
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 * ------------------------------------------------------------------------
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * void echttp_static_map (const char *uri, const char *path);
 *
 *    Declare a mapping between an URI and a local file or folder.
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

void echttp_static_map (const char *uri, const char *path) {
}

const char *echttp_static_page (const char *action,
                                const char *uri,
                                const char *data, int length) {
    return 0;
}

