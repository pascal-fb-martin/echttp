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
 * echttp_tls.c -- a TLS connection management layer.
 *
 * int  echttp_tls_initialize (int size, int argc, const char **argv);
 * int  echttp_tls_attach (int client, int s, const char *host);
 *
 * int  echttp_tls_send (int client, const char *data, int length);
 * int  echttp_tls_transfer (int client, int fd, int offset);
 *
 * typedef int  echttp_tls_receiver (int client, char *data, int length);
 * int echttp_tls_ready (int client, int mode, echttp_tls_receiver *receiver);
 *
 * void echttp_tls_detach_client (int client, const char *reason);
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

#include "echttp.h"
#include "echttp_raw.h"
#include "echttp_tls.h"

#define ETH_MAX_FRAME 1500

#define ECHTTP_TLS_IDLE     0
#define ECHTTP_TLS_CONNECT  1
#define ECHTTP_TLS_TRANSFER 2

#define ECHTTP_CLIENT_BUFFER 102400
typedef struct {
    char data[ECHTTP_CLIENT_BUFFER];
    int start;
    int end;
} echttp_buffer;

typedef struct {
    SSL *ssl;
    int pending;
    int transfer_fd;
    int transfer_length;
    echttp_buffer out;
    echttp_buffer in;
} echttp_tls_registration;

static echttp_tls_registration **echttp_tls_clients = 0;
static int echttp_tls_clients_size = 0;

static SSL_CTX *echttp_tls_context = 0;

static const char *echttp_tls_certificates = "/etc/ssl/certs";
static int echttp_tls_debug = 0;

static void echttp_tls_start (void) {

    if (echttp_tls_context) return;

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    echttp_tls_context = SSL_CTX_new(TLS_client_method());
    if (!echttp_tls_context) {
        if (echttp_tls_debug) ERR_print_errors_fp(stderr);
        return;
    }
    SSL_CTX_set_verify(echttp_tls_context, SSL_VERIFY_PEER, 0);
    if (!SSL_CTX_load_verify_locations
            (echttp_tls_context, 0, echttp_tls_certificates)) {
        if (echttp_tls_debug) printf ("Failed to load certificates\n");
        return;
    }
    if (echttp_tls_debug) printf ("TLS module started\n");
}

static void echttp_tls_cleanup (int client) {

    echttp_tls_registration *registered = echttp_tls_clients[client];
    if (registered->ssl) SSL_free (registered->ssl);
    registered->ssl = 0;
    registered->pending = ECHTTP_TLS_IDLE;
    if (registered->transfer_fd >= 0) close (registered->transfer_fd);
    registered->transfer_fd = -1;
    registered->transfer_length = 0;
    registered->in.start = registered->in.end = 0;
    registered->out.start = registered->out.end = 0;
}

int echttp_tls_initialize (int size, int argc, const char **argv) {

    int i;
    int shift;

    for (i = 1, shift = 1; i < argc; ++i) {
       if (shift != i) argv[shift] = argv[i];
       if (echttp_option_match
               ("-tls-certs=", argv[i], &echttp_tls_certificates)) continue;

       if (echttp_option_present ("-tls-debug", argv[i])) {
           echttp_tls_debug = 1;
           continue;
       }
       shift += 1;
    }
    if (echttp_isdebug()) echttp_tls_debug = 1;

    echttp_tls_clients = calloc (size, sizeof(*echttp_tls_clients));
    echttp_tls_clients_size = size;
    if (echttp_tls_debug) printf ("TLS module initialized\n");
    return shift;
}

static int echttp_tls_connect (int client) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

    ERR_clear_error();
    int ret = SSL_connect(registered->ssl);

    registered->pending = ECHTTP_TLS_CONNECT; // Default.

    switch (SSL_get_error(registered->ssl, ret)) {
        case SSL_ERROR_NONE:
            if (echttp_tls_debug) printf ("SSL_connect completed\n");
            if (registered->transfer_length > 0) {
                registered->pending = ECHTTP_TLS_TRANSFER;
                return 2;
            }
            registered->pending = ECHTTP_TLS_IDLE;
            return (registered->out.end > registered->out.start)?2:0;
        case SSL_ERROR_ZERO_RETURN:
            if (echttp_tls_debug)
                printf ("SSL_connect: zero return, error %s\n",
                        ERR_error_string(ERR_get_error(), 0));
            echttp_tls_cleanup (client);
            return -1;
        case SSL_ERROR_WANT_READ:
            if (echttp_tls_debug) printf ("SSL_connect: read blocked\n");
            return 0;
        case SSL_ERROR_WANT_ASYNC:
            if (echttp_tls_debug) printf ("SSL_connect: async\n");
            return 0;
        case SSL_ERROR_WANT_WRITE:
            if (echttp_tls_debug) printf ("SSL_connect: write blocked\n");
            return 2;
        case SSL_ERROR_WANT_CONNECT:
            if (echttp_tls_debug) printf ("SSL_connect: connect blocked\n");
            return 2;
        case SSL_ERROR_WANT_X509_LOOKUP:
            if (echttp_tls_debug) printf ("SSL_connect: bad certificate\n");
            echttp_tls_cleanup (client);
            return -1;
        case SSL_ERROR_SYSCALL:
            if (echttp_tls_debug) {
                printf ("SSL_connect: system error %s\n", strerror(errno));
                ERR_print_errors_fp(stderr);
            }
            echttp_tls_cleanup (client);
            return -1;
        case SSL_ERROR_SSL:
            if (echttp_tls_debug) {
                printf ("SSL_connect: unrecoverable error %s\n",
                        ERR_error_string(ERR_get_error(), 0));
                ERR_print_errors_fp(stderr);
            }
            echttp_tls_cleanup (client);
            return -1;
        default:
            if (echttp_tls_debug) printf ("SSL_connect: unexpected error\n");
            echttp_tls_cleanup (client);
            return -1;
    }
    return 0;
}

int echttp_tls_attach (int client, int s, const char *host) {

    if (echttp_tls_debug)
        printf ("TLS attaching client %d to socket %d\n", client, s);
    if (!echttp_tls_clients) {
        if (echttp_tls_debug) printf ("TLS module not initialized\n");
        return -1;
    }
    if (s < 0) {
        if (echttp_tls_debug) printf ("invalid socket\n");
        return -1;
    }
    if (client < 0 || client >= echttp_tls_clients_size) {
        if (echttp_tls_debug) printf ("invalid client\n");
        return -1;
    }

    echttp_tls_registration *registered = echttp_tls_clients[client];

    if (!registered) {
        registered = malloc(sizeof(**echttp_tls_clients));
        registered->ssl = 0;
        registered->transfer_fd = -1;
        echttp_tls_clients[client] = registered;
    }
    echttp_tls_cleanup (client);

    echttp_tls_start ();
    registered->ssl = SSL_new(echttp_tls_context);
    if (!registered->ssl) {
        if (echttp_tls_debug) {
            printf ("SSL_new failed\n");
            ERR_print_errors_fp(stderr);
        }
        return -1;
    }
    SSL_set_min_proto_version(registered->ssl, TLS1_VERSION);
    SSL_set_tlsext_host_name (registered->ssl, host);

    if (!SSL_set_fd(registered->ssl, s)) {
        if (echttp_tls_debug) {
            printf ("SSL_set_fd failed\n");
            ERR_print_errors_fp(stderr);
        }
        return -1;
    }
    return echttp_tls_connect (client);
}

static int echttp_tls_consume (echttp_buffer *buffer, int length) {
    buffer->start += length;
    if (buffer->start >= buffer->end) {
        buffer->start = buffer->end = 0;
        if (echttp_tls_debug) printf ("Buffer empty\n");
    }
    return (buffer->end == 0);
}

static int echttp_tls_flush (int client) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

    if (registered->pending == ECHTTP_TLS_CONNECT) return 2;

    int length = registered->out.end - registered->out.start;

    if (length <= 0) {
        registered->out.start = registered->out.end = 0;
        return 0;
    }
    if (echttp_tls_debug) printf ("Client %d flushing\n", client);
    ERR_clear_error();
    int ret = SSL_write (registered->ssl,
                         registered->out.data + registered->out.start, length);

    switch (SSL_get_error(registered->ssl, ret)) {
        case SSL_ERROR_NONE:
            if (echttp_tls_debug) printf ("SSL_write completed\n");
            if (echttp_tls_consume (&(registered->out), ret)) return 0;
            return 2;
        case SSL_ERROR_ZERO_RETURN:
            if (echttp_tls_debug)
                printf ("SSL_connect: zero return, error %s\n",
                        ERR_error_string(ERR_get_error(), 0));
            echttp_tls_cleanup (client);
            return -1;
        case SSL_ERROR_WANT_READ:
            if (echttp_tls_debug) printf ("SSL_write: read blocked\n");
            return 1;
        case SSL_ERROR_WANT_WRITE:
            if (echttp_tls_debug) printf ("SSL_write: write blocked\n");
            return 2;
        default:
            if (echttp_tls_debug) printf ("SSL_write: unknown error code\n");
            echttp_tls_cleanup (client);
            return -1;
    }
    return 0; // We should never get there.
}

static int echttp_tls_store (int client, const char *data, int length) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

    int available = sizeof(registered->out.data) - registered->out.end;

    if (length > available) length = available;
    if (length > 0) {
        memcpy (registered->out.data + registered->out.end, data, length);
        registered->out.end += length;
    } else {
        if (echttp_tls_debug) printf ("Client %d: buffer full\n", client);
    }
    return length;
}

int echttp_tls_send (int client, const char *data, int length) {

    if (echttp_tls_debug) printf ("TLS send %d bytes: %s\n", length, data);

    length = echttp_tls_store (client, data, length);
    echttp_tls_flush (client);
    return length;
}

static int echttp_tls_receive (int client, echttp_tls_receiver *receiver) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

    if (registered->pending == ECHTTP_TLS_CONNECT) return 0;

    char *cursor = registered->in.data + registered->in.start;
    int   available = sizeof(registered->in.data) - registered->in.end - 1;

    if (available <= 0) {
        if (echttp_tls_debug)
            printf ("TLS client %d: in buffer full\n", client);
        return -1;
    }

    if (echttp_tls_debug) printf ("Client %d reading\n", client);
    ERR_clear_error();
    int ret = SSL_read (registered->ssl, cursor, available);

    switch (SSL_get_error(registered->ssl, ret)) {
        case SSL_ERROR_NONE:
            registered->in.end += ret;
            registered->in.data[registered->in.end] = 0; // Terminate string.
            if (echttp_tls_debug)
                printf ("Client %d TLS data: %s\n", client, cursor);
            if (receiver) {
                cursor = registered->in.data + registered->in.start;
                available = registered->in.end - registered->in.start;
                ret = receiver (client, cursor, available);
                echttp_tls_consume (&(registered->in), ret);
            }
            return 1;
        case SSL_ERROR_ZERO_RETURN:
            if (echttp_tls_debug)
                printf ("SSL_connect: zero return, error %s\n",
                        ERR_error_string(ERR_get_error(), 0));
            echttp_tls_cleanup (client);
            return -1;
        case SSL_ERROR_WANT_READ:
            if (echttp_tls_debug) printf ("SSL_read: read blocked\n");
            return 1;
        case SSL_ERROR_WANT_WRITE:
            if (echttp_tls_debug) printf ("SSL_read: write blocked\n");
            return 2;
        default:
            if (echttp_tls_debug) printf ("SSL_read: unknown error code\n");
            echttp_tls_cleanup (client);
            return -1;
    }
    return 0; // We should never get there.
}

int echttp_tls_transfer (int client, int fd, int length) {

    echttp_tls_registration *registered = echttp_tls_clients[client];
    if (registered->transfer_fd >= 0) close (registered->transfer_fd);
    registered->transfer_fd = fd;
    registered->transfer_length = length;
    if (registered->pending == ECHTTP_TLS_IDLE)
        registered->pending = ECHTTP_TLS_TRANSFER;
    return 2;
}

static int echttp_tls_transmit (int client) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

    int length = registered->transfer_length;

    if (length <= 0) {
        registered->transfer_length = 0;
        if (registered->pending == ECHTTP_TLS_TRANSFER)
            registered->pending = ECHTTP_TLS_IDLE;
        return echttp_tls_flush(client);
    }
    if (registered->pending == ECHTTP_TLS_IDLE)
        registered->pending = ECHTTP_TLS_TRANSFER;

    int available = sizeof(registered->out.data) - registered->out.end;
    if (available <= 0) return 2;

    if (length > available) length = available;

    length = read (registered->transfer_fd,
                   registered->out.data + registered->out.end, length);
    if (length <= 0) {
        close (registered->transfer_fd);
        registered->transfer_fd = -1;
        registered->transfer_length = 0;
        if (registered->pending == ECHTTP_TLS_TRANSFER)
            registered->pending = ECHTTP_TLS_IDLE;
        return 0;
    }
    registered->out.end += length;

    registered->transfer_length -= length;
    if (registered->transfer_length <= 0) {
        close (registered->transfer_fd);
        registered->transfer_fd = -1;
        registered->transfer_length = 0;
        if (registered->pending == ECHTTP_TLS_TRANSFER)
            registered->pending = ECHTTP_TLS_IDLE;
        return echttp_tls_flush (client);
    }
    echttp_tls_flush (client);
    return 2; // We have more data to send, even if the buffer is empty.
}

int echttp_tls_ready (int client, int mode, echttp_tls_receiver *receiver) {

    echttp_tls_registration *registered = echttp_tls_clients[client];

     switch (registered->pending) {
        case ECHTTP_TLS_CONNECT:
            if (echttp_tls_debug)
                printf ("Client %d: retrying SSL_connect()\n", client);
            return echttp_tls_connect(client);
        case ECHTTP_TLS_TRANSFER:
            if (echttp_tls_debug)
                printf ("Client %d: retrying transfer\n", client);
            if (mode & 2) return (mode & 1) | echttp_tls_transmit(client);
            return (mode & 1);
        case ECHTTP_TLS_IDLE:
            if (echttp_tls_debug)
                printf ("Client %d: flush\n", client);
            switch (mode) {
                case 1:
                    return echttp_tls_receive (client, receiver);
                case 2:
                    return echttp_tls_flush(client);
                case 3:
                    return echttp_tls_flush(client)
                         | echttp_tls_receive (client, receiver);
            }
    }
    return 0;
}

void echttp_tls_detach_client (int i, const char *reason) {

    if (echttp_tls_clients[i]->ssl != 0) {
        if (echttp_tls_debug) printf ("closing TLS client %d: %s\n", i, reason);
        echttp_tls_cleanup(i);
    }
}

