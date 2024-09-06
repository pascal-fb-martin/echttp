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
 * A small toolkit to create "live" sorted lists
 */

struct echttp_sorted_bucket;
typedef struct echttp_sorted_bucket *echttp_sorted_list;

typedef void echttp_sorted_action (void *data);

echttp_sorted_list echttp_sorted_new (void);

void echttp_sorted_add (echttp_sorted_list b,
                        unsigned long long key, void *data);
void echttp_sorted_remove (echttp_sorted_list b,
                           unsigned long long key, void *data);

void echttp_sorted_descending (echttp_sorted_list b,
                               echttp_sorted_action *action);
void echttp_sorted_ascending (echttp_sorted_list b,
                              echttp_sorted_action *action);

void echttp_sorted_audit (echttp_sorted_list b, int *buckets, int *items);
