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
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp_catalog.h"


/* This hash function is derived from Daniel J. Bernstein's djb2 hash function.
 *
 * It was made case independent because several HTTP entities are not case
 * sensitive. We do not apply the modulo here: the full hash value will be
 * later to accelerate srting search when there is collision.
 */
unsigned int echttp_catalog_signature (const char *name) {

    unsigned int hash = 5381;
    int c;
    while (c = *name++)
        hash = ((hash << 5) + hash) + tolower(c); /* hash * 33 + c */
    return hash;
}

void echttp_catalog_reset (echttp_catalog *d) {

    int i;
    for (i = 1; i < ECHTTP_MAX_SYMBOL; ++i) {
        d->item[i].name = d->item[i].value = 0;
        d->item[i].next = 0;
    }
    for (i = 0; i < ECHTTP_HASH; ++i) {
        d->index[i] = 0;
    }
    d->count = 0;
}

void echttp_catalog_add (echttp_catalog *d,
                         const char *name, const char *value) {

    int index = d->count + 1;
    unsigned int signature = echttp_catalog_signature(name);
    int hash = signature % ECHTTP_HASH;

    if (index >= ECHTTP_MAX_SYMBOL) {
        fprintf (stderr, "Too many parameters.\n");
        return;
    }
    d->item[index].name = name;
    d->item[index].value = value;
    d->item[index].signature = signature;
    d->item[index].next = d->index[hash];
    d->index[hash] = index;
    d->count = index;
}

const char *echttp_catalog_get (echttp_catalog *d, const char *name) {

    int i;
    unsigned int signature = echttp_catalog_signature(name);
    int hash = signature % ECHTTP_HASH;
    for (i = d->index[hash]; i > 0; i = d->item[i].next) {
        if (d->item[i].signature != signature) continue;
        if (strcasecmp(name, d->item[i].name) == 0) {
            return d->item[i].value;
        }
    }
    return 0;
}

