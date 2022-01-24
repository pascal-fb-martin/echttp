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
 * Manage a catalog of symbols. This is basically a specialized hash table,
 * where the values are printable strings.
 *
 * void echttp_catalog_reset (echttp_catalog *d);
 *
 *    Erase all data in the given catalog. After this the catalog is empty.
 *
 * void echttp_catalog_set (echttp_catalog *d,
 *                          const char *name, const char *value);
 *
 *    Insert a new item, or change its value.
 *
 * const char *echttp_catalog_refresh (echttp_catalog *d,
 *                                     const char *name, const char *value,
 *                                     time_t timestamp);
 *
 *    Insert a new item, or change its value and update its timestamp.
 *    This function returns the old value of the item, if it was found,
 *    or else 0.
 *    (The timestamp can be used by the application to detect old items
 *    that can be ignored.)
 *
 * const char *echttp_catalog_get (echttp_catalog *d, const char *name);
 *
 *    Retrieve the value associated with the provided key. Returns 0 when
 *    the key is not found.
 *
 * void echttp_catalog_join (echttp_catalog *d,
 *                           const char *sep, char *text, int size);
 *
 *    Create an ASCII list of all the items present in a catalog. All names
 *    are encoded using the HTTP encoding rules.
 *
 * void echttp_catalog_enumerate (echttp_catalog *d,
 *                                echttp_catalog_action *action);
 *
 *    Call action for each item of the catalog.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp_encoding.h"
#include "echttp_catalog.h"


void echttp_catalog_reset (echttp_catalog *d) {
    echttp_hash_reset (d, 0);
}

const char *echttp_catalog_refresh (echttp_catalog *d,
                                    const char *name, const char *value, time_t timestamp) {

    return (const char *)
               echttp_hash_refresh (d, name, (void *)value, timestamp);
}

void echttp_catalog_set (echttp_catalog *d,
                         const char *name, const char *value) {
    echttp_hash_set (d, name, (void *) value);
}

const char *echttp_catalog_get (echttp_catalog *d, const char *name) {

    return (const char *) echttp_hash_get (d, name);
}

void echttp_catalog_join (echttp_catalog *d,
                          const char *sep, char *text, int size) {

    int i;
    int length = 0;

    text[0] = 0;

    for (i = 1; i <= d->count; ++i) {
        char encoded1[127];
        char encoded2[127];
        echttp_encoding_escape (d->item[i].name, encoded1, sizeof(encoded1));
        echttp_encoding_escape ((char *)(d->item[i].value), encoded2, sizeof(encoded2));
        snprintf (text+length, size-length,
                  "%s%s=%s", length?sep:"", encoded1, encoded2);
        length += strlen(text+length);
    }
}

void echttp_catalog_enumerate (echttp_catalog *d,
                               echttp_catalog_action *action) {

    int i;

    for (i = 1; i <= d->count; ++i) {
        action (d->item[i].name, (const char *)(d->item[i].value));
    }
}

