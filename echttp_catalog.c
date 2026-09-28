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
 * void echttp_catalog_create (echttp_catalog *d);
 *
 *    Create a new catalog. This disregards any data held in the provided
 *    catalog structure.
 *
 * void echttp_catalog_reset (echttp_catalog *d);
 *
 *    Erase all data in the given catalog. After this, the catalog is empty.
 *
 * char *echttp_catalog_set (echttp_catalog *d,
 *                           const char *name, const char *value);
 *
 *    Insert a new item, or change its value.
 *    Return the item's previous value, or else null. The returned pointer
 *    is not a constant: the reference was removed from the catalog and
 *    the caller is allowed to do what it sees fit with it (like free it).
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
 *    Call action for each item of the catalog. Stop at the end of the catalog
 *    Or when the action returns true.
 *
 * void echttp_catalog_release (echttp_catalog *d);
 *
 *    Release all resources allocated for this catalog. This is a simplified
 *    variant of echttp_catalog_free(), when the caller has nothing to free.
 *
 * void echttp_catalog_free (echttp_catalog *d, echttp_catalog_action *action);
 *
 *    Release all resources allocated for this catalog by this module or the
 *    caller. The action allows the caller to free its own resources.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp_encoding.h"
#include "echttp_catalog.h"

#define ECHTTP_MAX_SYMBOL 256

void echttp_catalog_create (echttp_catalog *d) {
    echttp_hash_create (d, ECHTTP_MAX_SYMBOL);
}

void echttp_catalog_reset (echttp_catalog *d) {
    echttp_hash_reset (d, 0);
}

void echttp_catalog_release (echttp_catalog *d) {
    echttp_catalog_reset (d);
    echttp_hash_release (d);
}

void echttp_catalog_free (echttp_catalog *d, echttp_catalog_action *action) {
    if (action) echttp_catalog_enumerate (d, action);
    echttp_catalog_release (d);
}

char *echttp_catalog_set (echttp_catalog *d,
                          const char *name, const char *value) {
    return (char *) echttp_hash_set (d, name, (void *) value);
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
        int done = action (d->item[i].name, (const char *)(d->item[i].value));
        if (done) return;
    }
}

