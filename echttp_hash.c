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
 * A small, low memory footprint toolkit to create hash tables.
 *
 * (The current implementation is a transition step, to maintain compatibility
 * with a few applications and until these applications have migrated to the
 * new API. The description below describes the final design.)
 *
 * This module provides a way to associate a string key with an array index.
 * It is up to the application to declare the array with a data structure
 * appropriate to their needs.
 *
 * This module supports two types of index: unique keys and non-unique keys.
 *
 * On a unique key index, each key appears only once. Trying to create an
 * already existing key causes the index for the existing key to be returned.
 * This type of index can be used to create a key/value store (see file
 * echttp_catalog.c).
 *
 * On a non-unique key index, each key may appear more than once. It is
 * possible to walk the list of records matching a specific key. This
 * type of index is very useful to optimize searches like "retrieve all
 * the children of a specific parent".
 *
 * unsigned int echttp_hash_signature (const char *name);
 *
 *    Calculate a signature for the provided key. A signature is the hash
 *    code _before_ applying the modulo operation. In this respect it is
 *    a more unique and discriminant value that the hash code. It is also
 *    independent from the hash modulo (if ever that one changes when the
 *    table expands).
 *
 * void echttp_hash_reset (echttp_hash *d, echttp_hash_action *action);
 *
 *    Erase all data in the given hash. After this call the hash is empty.
 *    The action may be used to free name and value if required.
 *    A null action is ignored.
 *
 * int echttp_hash_find (echttp_hash *d, const char *name);
 *
 *    Return 0 if the item was not found, the item's index if found.
 *
 * int echttp_hash_next (echttp_hash *d, int from, const char *name);
 *
 *    Return 0 if no other matching item was found after from, the item's
 *    index if found. (This is used for non-unique indexes only.)
 *
 * int echttp_hash_add (echttp_hash *d, const char *name);
 *
 *    Add a new item, even if the key already exists.
 *    Return 0 if the hash table is full, the new item's index otherwise.
 *    (This is used for non-unique indexes only.)
 *
 * int echttp_hash_iterate (echttp_hash *d,
 *                          const char *name, echttp_hash_action *action);
 *
 *    Scan the items in the hash index and call action for each matching
 *    items. If name is null, action is called for all items in the hash
 *    index. The action must not be null (what would be the point?).
 *    If the action returns 0, the iteration continue until no more item
 *    is found; otherwise the iteration stops and returns the last
 *    processed index.
 *
 * void echttp_hash_insert (echttp_hash *d, const char *name);
 *
 *    Insert a new item if it did not exist already. Return 0 if the hash
 *    table is full and cannot accomodate any new item, or the item's index
 *    otherwise (This is used for unique indexes only.)
 *
 * void echttp_hash_set (echttp_hash *d,
 *                       const char *name, const char *value);
 *
 *    Insert a new item, or change the value of an item that already exists.
 *    (Compatibility API, to be phased out.)
 *
 * const char *echttp_hash_refresh (echttp_hash *d,
 *                                  const char *name, void *value,
 *                                  time_t timestamp);
 *
 *    Same as echttp_hash_set, but also update the item's timestamp.
 *    (The timestamp can be used by the application to detect old items
 *    that should be ignored.)
 *    TBD: the timestamp should really be managed by the caller..
 *    (Compatibility API, to be phased out.)
 *
 * void *echttp_hash_get (echttp_hash *d, const char *name);
 *
 *    Retrieve the value associated with the provided key. Returns 0 when
 *    the key is not found. If there are duplicates items, the value of
 *    the first one found is returned: do not use with non-unique indexes.
 *    (Compatibility API, to be phased out.)
 *
 * LIMITATIONS
 *
 * The current implementation is limited to a maximum of 256 entries. This
 * module was never meant to be used for datasets with millions of items.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp_hash.h"


/* This hash function is derived from Daniel J. Bernstein's djb2 hash function.
 *
 * It was made case independent because several HTTP entities are not case
 * sensitive. We do not apply the modulo here: the full hash value will be
 * used later to accelerate string search when there is collision.
 */
unsigned int echttp_hash_signature (const char *name) {

    unsigned int hash = 5381;
    int c;
    while (c = *name++)
        hash = ((hash << 5) + hash) + tolower(c); /* hash * 33 + c */
    return hash;
}

void echttp_hash_reset (echttp_hash *d, echttp_hash_action *action) {

    int i;
    for (i = 1; i < ECHTTP_MAX_SYMBOL; ++i) {
        if (d->item[i].name && action)
            action (i, d->item[i].name);
        d->item[i].name = d->item[i].value = 0;
        d->item[i].next = 0;
        d->item[i].timestamp = 0;
    }
    for (i = 0; i < ECHTTP_HASH; ++i) {
        d->index[i] = 0;
    }
    d->count = 0;
}

static int echttp_hash_forage (echttp_hash *d, int start,
                               const char *name, int signature) {
    int i;
    for (i = start; i > 0; i = d->item[i].next) {
        if (d->item[i].signature != signature) continue;
        if (strcasecmp(name, d->item[i].name) == 0) {
            return i;
        }
    }
    return 0;
}

int echttp_hash_find (echttp_hash *d, const char *name) {
    unsigned int signature = echttp_hash_signature(name);
    int hash = signature % ECHTTP_HASH;
    return echttp_hash_forage (d, d->index[hash], name, signature);
}

int echttp_hash_next (echttp_hash *d, int from, const char *name) {
    if (from <= 0) return 0;
    return echttp_hash_forage (d, d->item[from].next,
                               name, d->item[from].signature);
}

int echttp_hash_add (echttp_hash *d, const char *name) {

    unsigned int signature = echttp_hash_signature(name);
    int hash = signature % ECHTTP_HASH;
    int index = d->count + 1;

    if (index >= ECHTTP_MAX_SYMBOL) {
        fprintf (stderr, "Too many symbols in hash.\n");
        return 0;
    }
    d->item[index].name = name;
    d->item[index].value = 0;
    d->item[index].signature = signature;
    d->item[index].next = d->index[hash];
    d->index[hash] = index;
    d->count = index;

    return index;
}

int echttp_hash_iterate (echttp_hash *d,
                         const char *name, echttp_hash_action *action) {
    int i;
    if (!name) {
        for (i = 1; i < d->count; ++i) {
            if (action (i, d->item[i].name)) return i;
        }
    } else {
        for (i = echttp_hash_find (d, name);
             i > 0; i = echttp_hash_next(d, i, name)) {
            if (action (i, name)) return i;
        }
    }
    return 0;
}

int echttp_hash_insert (echttp_hash *d, const char *name) {

    int index = echttp_hash_find (d, name);

    if (index <= 0) {
        index = echttp_hash_add (d, name);
    }
    return index;
}

void *echttp_hash_refresh (echttp_hash *d,
                           const char *name, void *value, time_t timestamp) {

    int index = echttp_hash_insert (d, name);
    void *old;

    if (index > 0) {
        old = d->item[index].value;
        d->item[index].value = value;
        d->item[index].timestamp = timestamp;
    } else {
        old = 0;
    }
    return old;
}

void echttp_hash_set (echttp_hash *d, const char *name, void *value) {
    echttp_hash_refresh (d, name, value, 0);
}

void *echttp_hash_get (echttp_hash *d, const char *name) {

    int i = echttp_hash_find (d, name);
    if (i > 0) return d->item[i].value;
    return 0;
}

