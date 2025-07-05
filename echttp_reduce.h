/* echttp - Embedded HTTP server.
 *
 * Copyright 2025, Pascal Martin
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
 * echttp_reduce.h - Generate quantil representations of metrics series.
 */

void echttp_reduce_percentage (long long reference, int count,
                               long long *in, long long *out);

int echttp_reduce_json (char *buffer, int size,
                        const char *name,
                        long long *values, int count, const char *unit);

int echttp_reduce_details_json (char *buffer, int size, time_t since,
                                const char *name, const char *unit,
                                time_t now, int step, int count,
                                time_t *timestamps, long long *values);

