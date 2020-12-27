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
 * echttp_tls.h -- a TLS connection management layer.
 */

void echttp_tls_initialize (int size);
int  echttp_tls_attach (int client, int s, const char *host);

int  echttp_tls_send (int client, const char *data, int length);
int  echttp_tls_transfer (int client, int fd, int offset);

typedef int echttp_tls_receiver (int client, char *data, int length);
int  echttp_tls_ready (int client, int mode, echttp_tls_receiver *receiver);

void echttp_tls_detach_client (int client, const char *reason);

