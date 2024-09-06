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
 * DESCRIPTION
 *
 * This is a small toolkit to create "live" sorted lists
 *
 * This module provides a way to maintain a sorted list while adding
 * and removing elements. Iterator functions are used to walk the list
 * in ascending or descending order.
 *
 * The sort key is a 64 bit unsigned integer. This module is specifically
 * tuned for sorting by timestamps.
 *
 * This module only contains an opaque reference to the actual data, and
 * never access this data: it only uses the sorting keys. It is the
 * application's responsibility to handle data storage.
 *
 * The sort key does not need to be unique: this module maintains the insert
 * order when the keys are identical (because of the tuning for chronological
 * order). However, a reference must remain unique in the list: the same
 * reference should not be added twice.
 *
 * SYNOPSYS
 *
 * echttp_sorted_list echttp_sorted_new (void);
 *
 *    Create a new empty sorted list.
 *
 * void echttp_sorted_add (echttp_sorted_list b,
 *                         unsigned long long key, void *data);
 *
 *    Add a new item to the list, keeping the list sorted.
 *
 * void echttp_sorted_remove (echttp_sorted_list b,
 *                            unsigned long long key, void *data);
 *
 *    Remove the specified item from the list, keeping the list sorted.
 *    Return 1 when the end of the list was reached, 0 when the walk was
 *    stopped before the end.
 *
 * int echttp_sorted_descending (echttp_sorted_list b,
 *                               echttp_sorted_action *action);
 *
 *    An iterator that walks the sorted list in descending order.
 *    The walk stops at the end of the list, or else when action returns 0.
 *    Return 1 when the end of the list was reached, 0 when the walk was
 *    stopped before the end.
 *
 * int echttp_sorted_ascending (echttp_sorted_list b,
 *                              echttp_sorted_action *action);
 *
 *    An iterator that walks the sorted list in ascending order.
 *    The walk stops at the end of the list, or else when action returns 0.
 *    Return 1 when the end of the list was reached, 0 when the walk was
 *    stopped before the end.
 *
 * int echttp_sorted_ascending_from (echttp_sorted_list b,
 *                                   unsigned long long key,
 *                                   echttp_sorted_action *action);
 *
 *    An iterator that walks the sorted list in ascending order, starting
 *    with the provided key value. This means that action will only be called
 *    for items associated with keys greater or equal than the provided value.
 *    The goal is to save the overhead thatcomes with walking the whole list
 *    everytime there is a request for newer items.
 *    The walk stops at the end of the list, or else when action returns 0.
 *    Return 1 when the end of the list was reached, 0 when the walk was
 *    stopped before the end.
 *
 * void echttp_sorted_audit (echttp_sorted_list b, int *buckets, int *items);
 *
 *    Calculate the number of buckets and items currently allocated.
 *    This function is intended for unit testing purpose.
 *
 * IMPLEMENTATION
 *
 * A list is represented as a tree of buckets, 8 levels deep. Each level
 * represents one byte from the key: the first (top) level represents the
 * most significant bytes while the last (bottom level represents the least
 * significant bytes. Buckets at the last level points to collision lists
 * instead of buckets: all the items that share the same key are listed
 * in the order they were added.
 *
 * What is expected to happen when using live timestamps is the creation of
 * a small number of low depth buckets, since all the timestamps share the
 * same most significant bytes values, followed by a "fan" of mostly filled
 * high depth buckets. Imagine a palm tree.. As time goes by, the older
 * entries are removed and more recent new entries are created, keeping
 * the overall palm tree shape.
 *
 * LIMITATIONS
 *
 * This implementation would not work well with randomized keys: it could
 * cause the creation of a lot of sparce buckets, wasting memory. However
 * this fits very well with timestamps, as "live" timestamps tend to be
 * aggregated around the current day/month/year.
 *
 * There is no optimization intended to reduce the depth of the tree of
 * buckets at that time. Such an optimization would be to skip depth levels
 * that have the same value for all items. Such an optimization could make
 * add/remove operations faster on average when using live timestamps. It
 * was decided to not do it because the saving in term of buckets would be
 * small (6 buckets or less) anyway, but this would significantly increase
 * the implementation's complexity.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp_sorted.h"


struct echttp_sorted_leaf {
    struct echttp_sorted_leaf *prev;
    struct echttp_sorted_leaf *next;
    void *data;
};

struct echttp_sorted_bucket {
    int depth;
    union {
        struct echttp_sorted_bucket *sub;  // if depth < 7
        struct echttp_sorted_leaf   *leaf; // if depth == 7
    } index[256];
};

static struct echttp_sorted_leaf *echttp_sorted_new_leaf (void *data) {
    struct echttp_sorted_leaf *leaf = malloc (sizeof(struct echttp_sorted_leaf));
    leaf->prev = 0;
    leaf->next = 0;
    leaf->data =data;
    return leaf;
}

static struct echttp_sorted_bucket *echttp_sorted_new_bucket (int depth) {
    struct echttp_sorted_bucket *bucket = malloc (sizeof(struct echttp_sorted_bucket));
    bucket->depth = depth;
    int i;
    for (i = 255; i >= 0; --i) bucket->index[i].sub = 0;
    return bucket;
}

static void echttp_sorted_add_leaf (struct echttp_sorted_leaf *l, void *data) {
    struct echttp_sorted_leaf *leaf = echttp_sorted_new_leaf (data);
    leaf->prev = l->next;
    if (l->next) {
        l->next->next = leaf; // At the end of an existing list.
    } else {
        l->prev = leaf; // The list was empty: this is the 1st element.
    }
    l->next = leaf; // This is the new end of the list.
}

echttp_sorted_list echttp_sorted_new (void) {
    return echttp_sorted_new_bucket (0);
}

void echttp_sorted_add (echttp_sorted_list b, unsigned long long key, void *data) {
    if (b->depth >= 7) {
        int hash = key & 0xff;
        if (!b->index[hash].leaf) {
            b->index[hash].leaf = echttp_sorted_new_leaf(0);
        }
        echttp_sorted_add_leaf (b->index[hash].leaf, data);
    } else {
        int hash = (key >> (8*(7-b->depth))) & 0xff;
        if (!b->index[hash].sub) {
            b->index[hash].sub = echttp_sorted_new_bucket (b->depth+1);
        }
        echttp_sorted_add (b->index[hash].sub, key, data);
    }
}

static int echttp_sorted_bucket_empty (struct echttp_sorted_bucket *b) {
    int i;
    for (i = 255; i >= 0; --i) if (b->index[i].sub) return 0;
    return 1;
}

void echttp_sorted_remove (echttp_sorted_list b, unsigned long long key, void *data) {
    int i;
    int hash;
    struct echttp_sorted_bucket *stack[8];
    struct echttp_sorted_bucket *bucket = b;
    for (i = 7; i > 0; --i) {
        stack[i] = bucket;
        if (!bucket) return; // Nothing to remove if not present.
        hash = (key >> (8*i)) & 0xff;
        bucket = bucket->index[hash].sub;
    }
    if (!bucket) return;
    // We have now reached the deepest bucket, which points to leaves.
    hash = key & 0xff;
    struct echttp_sorted_leaf *leaf = bucket->index[hash].leaf;
    if (!leaf) return; // Nothing to remove from an empty list.
    struct echttp_sorted_leaf *cursor;
    for (cursor = leaf->prev; cursor; cursor = cursor->next) {
        if (cursor->data == data) break;
    }
    if (!cursor) return; // Not found, no change.

    // Remove from that collision list.
    if (cursor->prev) {
        cursor->prev->next = cursor->next;
    } else {
        leaf->prev = cursor->next;
    }
    if (cursor->next) {
        cursor->next->prev = cursor->prev;
    } else {
        leaf->next = cursor->prev;
    }
    free (cursor);
    if (leaf->prev || leaf->next) return; // List still not empty.

    // The leave list is now empty: remove it.
    free (leaf);
    bucket->index[hash].leaf = 0;

    // If the bucket is empty, delete it. Propagate through the stack, except
    // for the top element (which is the list itself).
    //
    for (i = 1; i <= 7; ++i) {
        if (!echttp_sorted_bucket_empty(bucket)) return;
        free(bucket);
        bucket = stack[i];
        hash = (key >> (8*i)) & 0xff;
        bucket->index[hash].sub = 0;
    }
}

static int echttp_sorted_descend_leaf (struct echttp_sorted_leaf *l,
                                       echttp_sorted_action *action) {
    struct echttp_sorted_leaf *cursor;
    for (cursor = l->next; cursor; cursor = cursor->prev) {
        if (!action(cursor->data)) return 0;
    }
    return 1;
}

int echttp_sorted_descending (echttp_sorted_list b,
                              echttp_sorted_action *action) {
    if (!b) return 1;
    int i;
    if (b->depth == 7) {
        for (i = 255; i >= 0; --i) {
            struct echttp_sorted_leaf *leaf = b->index[i].leaf;
            if (leaf) {
                if (!echttp_sorted_descend_leaf(leaf, action)) return 0;
            }
        }
    } else {
        for (i = 255; i >= 0; --i) {
            if (!echttp_sorted_descending (b->index[i].sub, action)) return 0;
        }
    }
    return 1;
}

static int echttp_sorted_ascend_leaf (struct echttp_sorted_leaf *l,
                                      echttp_sorted_action *action) {
    struct echttp_sorted_leaf *cursor;
    for (cursor = l->prev; cursor; cursor = cursor->next) {
        if (!action(cursor->data)) return 0;;
    }
    return 1;
}

int echttp_sorted_ascending (echttp_sorted_list b,
                             echttp_sorted_action *action) {
    if (!b) return 1;
    int i;
    if (b->depth == 7) {
        for (i = 0; i < 256; ++i) {
            struct echttp_sorted_leaf *leaf = b->index[i].leaf;
            if (leaf) {
                if (!echttp_sorted_ascend_leaf(leaf, action)) return 0;
            }
        }
    } else {
        for (i = 0; i < 256; ++i) {
            if (!echttp_sorted_ascending (b->index[i].sub, action)) return 0;
        }
    }
    return 1;
}

int echttp_sorted_ascending_from (echttp_sorted_list b,
                                  unsigned long long key,
                                  echttp_sorted_action *action) {
    if (!b) return 1;
    int i;
    if (b->depth == 7) {
        for (i = key & 0xff; i < 256; ++i) {
            struct echttp_sorted_leaf *leaf = b->index[i].leaf;
            if (leaf) {
                if (!echttp_sorted_ascend_leaf(leaf, action)) return 0;
            }
        }
    } else {
        // Walk only the relevant part of the first sub-bucket, but walk
        // all subsequent buckets in their entirety because they match
        // greater hash (i.e. greater key) values.
        //
        int hash = (key >> (8*(7-b->depth))) & 0xff;
        if (!echttp_sorted_ascending_from (b->index[hash].sub, key, action))
            return 0;
        for (i = hash+1; i < 256; ++i) {
            if (!echttp_sorted_ascending (b->index[i].sub, action)) return 0;
        }
    }
    return 1;
}

void echttp_sorted_audit (echttp_sorted_list b, int *buckets, int *items) {

    *buckets = 0;
    *items = 0;

    if (!b) return;
    int i;
    *buckets = 1; // This bucket, at least.
    if (b->depth == 7) {
        for (i = 255; i >= 0; --i) {
            struct echttp_sorted_leaf *leaf = b->index[i].leaf;
            if (!leaf) continue;
            struct echttp_sorted_leaf *cursor;
            for (cursor = leaf->next; cursor; cursor = cursor->prev) {
                *items += 1;
            }
        }
    } else {
        for (i = 255; i >= 0; --i) {
            int subbuckets;
            int subitems;
            echttp_sorted_audit (b->index[i].sub, &subbuckets, &subitems);
            *buckets += subbuckets;
            *items += subitems;
        }
    }
}

