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
 * A test for "live" sorted lists
 */

#include "time.h"
#include "stdio.h"
#include "echttp_sorted.h"

static int errors = 0;
#define title(t) printf ("== Line %d: %s\n", __LINE__, t);
#define comment(t, v) printf ("-- Line %d: %s = %ld\n", __LINE__, t, v);
#define assert(x, v, t) if ((x) != (v)) {printf ("** Line %d: %s = %ld\n", __LINE__, t, x); errors += 1;}

static int counter;

int ascent (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value, counter, "Unexpected ascending ordered item");
    counter += 1;
    return 1; // Keep going
}

int ascentstopped (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value, counter, "Unexpected ascending ordered item");
    counter += 1;
    return value < 2; // Stop when reaching 2
}

int descent (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value, counter, "Unexpected descending ordered item");
    counter -= 1;
    return 1; // Keep going
}

int descentstopped (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value, counter, "Unexpected descending ordered item");
    counter -= 1;
    return value > 2; // Stop when reaching 2
}

int ascentrandomized (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value > counter, 1, "Unexpected ascending ordered item");
    counter = value;
    return 1; // Keep going
}

int descentrandomized (void *data) {
    long value = (long)((long long)data);
    comment ("data", value);
    assert (value < counter, 1, "Unexpected ascending ordered item");
    counter = value;
    return 1; // Keep going
}

void check12 (echttp_sorted_list l) {

    int buckets;
    int items;

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 8, "Unexpected bucket count");
    assert (items, 2, "Unexpected item count");

    title ("Check descending iteration (2 items)");
    counter = 2;
    echttp_sorted_descending (l, descent);
    assert (counter, 0, "Unexpected counter leftover");

    title ("Check ascending iteration (2 items)");
    counter = 1;
    echttp_sorted_ascending (l, ascent);
    assert (counter, 3, "Unexpected counter leftover");
}

void check123 (echttp_sorted_list l) {

    int buckets;
    int items;

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 8, "Unexpected bucket count");
    assert (items, 3, "Unexpected item count");

    title ("Check descending iteration (3 items)");
    counter = 3;
    echttp_sorted_descending (l, descent);
    assert (counter, 0, "Unexpected counter leftover");

    title ("Check descending iteration down to 2 (2 items)");
    counter = 3;
    echttp_sorted_descending (l, descentstopped);
    assert (counter, 1, "Unexpected counter leftover");

    title ("Check ascending iteration (3 items)");
    counter = 1;
    echttp_sorted_ascending (l, ascent);
    assert (counter, 4, "Unexpected counter leftover");

    title ("Check ascending iteration up to 2 (2 items)");
    counter = 1;
    echttp_sorted_ascending (l, ascentstopped);
    assert (counter, 3, "Unexpected counter leftover");
}

void check1234 (echttp_sorted_list l) {

    int buckets;
    int items;

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 9, "Unexpected bucket count");
    assert (items, 4, "Unexpected item count");

    title ("Check descending iteration (4 items)");
    counter = 4;
    echttp_sorted_descending (l, descent);
    assert (counter, 0, "Unexpected counter leftover");

    title ("Check ascending iteration (4 items)");
    counter = 1;
    echttp_sorted_ascending (l, ascent);
    assert (counter, 5, "Unexpected counter leftover");
}

int main (int argc, char **argv) {

    int buckets;
    int items;

    time_t base = time(0) * 1000;
    echttp_sorted_list l = echttp_sorted_new ();

    echttp_sorted_add (l, base+1, (void *)1);
    echttp_sorted_add (l, base+2, (void *)2);
    echttp_sorted_add (l, base+3, (void *)3);

    check123 (l);

    echttp_sorted_add (l, base+257, (void *)4);

    check1234 (l);

    echttp_sorted_remove (l, base+257, (void *)4);

    check123 (l);

    echttp_sorted_remove (l, base+3, (void *)3);

    check12 (l);

    echttp_sorted_add (l, base+3, (void *)3);

    check123 (l);

    echttp_sorted_remove (l, base+2, (void *)2);

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 8, "Unexpected bucket count");
    assert (items, 2, "Unexpected item count");

    echttp_sorted_remove (l, base+3, (void *)3);

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 8, "Unexpected bucket count");
    assert (items, 1, "Unexpected item count");

    echttp_sorted_remove (l, base+1, (void *)1);

    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 1, "Unexpected bucket count");
    assert (items, 0, "Unexpected item count");

    unsigned long long randomized[10] = {3948294, 9483, 823, 84047592, 592856, 28452, 28453, 39684, 18346, 382945};

    int i;

    for (i = 0; i < 10; ++i) {
        echttp_sorted_add (l, base+randomized[i], (void *)randomized[i]);
    }
    title ("Check ascending iteration (randomized items)");
    counter = 0;
    echttp_sorted_ascending (l, ascentrandomized);
    assert (counter, 84047592, "Unexpected counter leftover");

    title ("Check ascending iteration from 28452 (randomized items)");
    counter = 28451;
    echttp_sorted_ascending_from (l, base+28452, ascentrandomized);
    assert (counter, 84047592, "Unexpected counter leftover");

    title ("Check descending iteration (randomized items)");
    counter = 84047593;
    echttp_sorted_descending (l, descentrandomized);
    assert (counter, 823, "Unexpected counter leftover");

    title ("Check descending iteration from 592856 (randomized items)");
    counter = 592857;
    echttp_sorted_descending_from (l, base+592856, descentrandomized);
    assert (counter, 823, "Unexpected counter leftover");

    echttp_sorted_audit (l, &buckets, &items);
    assert (items, 10, "Unexpected item count");

    for (i = 0; i < 10; ++i) {
        echttp_sorted_remove (l, base+randomized[i], (void *)randomized[i]);
    }
    echttp_sorted_audit (l, &buckets, &items);
    assert (buckets, 1, "Unexpected bucket count");
    assert (items, 0, "Unexpected item count");

    if (errors) {
        printf ("** Test failed, %d errors\n", errors);
    } else {
        printf ("== Test passed, no error\n");
    }
    return errors;
}
