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
 * echttp_raw.c -- a socket management, protocol-independent layer.
 *
 * typedef int echttp_raw_receiver (int client, char *data, int length);
 * int echttp_raw_open (const char *service, int debug);
 *
 * int echttp_raw_capacity (void);
 * int echttp_raw_server_port (int ip);
 *
 * int echttp_raw_send (int client, const char *data, int length);
 *
 * void echttp_raw_loop (echttp_raw_acceptor *acceptor,
 *                       echttp_raw_receiver *received);
 *
 * int echttp_raw_connect (const char *host, const char *service);
 *
 * void echttp_raw_close_client (int i, const char *reason);
 *
 * void echttp_raw_register (int fd, int mode,
 *                           echttp_listener *listener, int premium);
 * void echttp_raw_forget (int fd);
 *
 * void echttp_raw_close (void);
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ifaddrs.h>

#include "echttp.h"
#include "echttp_raw.h"

#define ETH_MAX_FRAME 1500

static int echttp_raw_server = -1;
static int echttp_raw_debug = 0;


#define ECHTTP_RAW_MAXREGISTERED  256
typedef struct {
    int mode;
    int premium;
    int fd;
    echttp_listener *listener;
} echttp_raw_registration;

static echttp_raw_registration echttp_raw_other[ECHTTP_RAW_MAXREGISTERED];
static int echttp_raw_other_count = 0;

static echttp_listener *echttp_raw_backgrounder = 0;

static int echttp_raw_serverport = 0;


#define ECHTTP_CLIENT_BUFFER 102400
typedef struct {
    char data[ECHTTP_CLIENT_BUFFER];
    int start;
    int end;
} echttp_buffer;

#define ECHTTP_CLIENT_MAX 32
static struct {
    int socket;
    struct sockaddr_in6 peer;
    time_t deadline;
    echttp_buffer in;
    echttp_buffer out;
    struct {
        int fd;
        int size;
    } transfer;
} echttp_raw_client[ECHTTP_CLIENT_MAX];


#define ECHTTP_IF_MAX 16
static struct {
    uint32_t ifaddr;
    uint32_t ifmask;
} echttp_raw_if[ECHTTP_IF_MAX];

static int echttp_raw_ifcount = 0;
static time_t echttp_raw_timestamp = 0;

static void echttp_raw_cleanup (int i) {
   echttp_raw_client[i].socket = -1;
   echttp_raw_client[i].in.start = 0;
   echttp_raw_client[i].in.end = 0;
   echttp_raw_client[i].out.start = 0;
   echttp_raw_client[i].out.end = 0;
   echttp_raw_client[i].deadline = 0;
   echttp_raw_client[i].transfer.fd = -1;
   echttp_raw_client[i].transfer.size = 0;
}

static const char *echttp_printip (long ip) {
    static char ascii[16];

    snprintf (ascii, sizeof(ascii), "%d.%d.%d.%d",
              (ip>>24) & 0xff, (ip>>16) & 0xff, (ip>>8) & 0xff, ip & 0xff);
    return ascii;
}

static void echttp_raw_enumerate (void) {

    struct ifaddrs *cards;
    time_t now = time(0);

    if (echttp_raw_timestamp + 10 > now) return; // Keep last result for 10s.

    if (getifaddrs(&cards) == 0) {

        struct ifaddrs *cursor;

        if (echttp_raw_debug)
            printf ("Network interfaces:\n");

        echttp_raw_ifcount = 0;

        for (cursor = cards; cursor != 0; cursor = cursor->ifa_next) {

            if (echttp_raw_debug)
                printf ("   name: %s:\n", cursor->ifa_name);

            if ((cursor->ifa_addr == 0) || (cursor->ifa_netmask == 0)) continue;
            if (cursor->ifa_addr->sa_family != AF_INET)  continue;

            // TBD: switch to IPv6.

            if (echttp_raw_debug) {
                struct sockaddr_in *ia;
                ia = (struct sockaddr_in *) (cursor->ifa_addr);
                printf ("      address: %s\n",
                        echttp_printip(ntohl((long)(ia->sin_addr.s_addr))));
                ia = (struct sockaddr_in *) (cursor->ifa_netmask);
                printf ("      mask: %s\n",
                        echttp_printip(ntohl((long)(ia->sin_addr.s_addr))));
            }

            if (echttp_raw_ifcount >= ECHTTP_IF_MAX) continue;

            echttp_raw_if[echttp_raw_ifcount].ifaddr =
                ((struct sockaddr_in *) cursor->ifa_addr)->sin_addr.s_addr;
            echttp_raw_if[echttp_raw_ifcount].ifmask =
                ((struct sockaddr_in *) cursor->ifa_netmask)->sin_addr.s_addr;
            echttp_raw_ifcount += 1;
        }
        freeifaddrs(cards);
        echttp_raw_timestamp = now;
    }
}

static int echttp_raw_new_client (void) {

   int i;

   for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
       if (echttp_raw_client[i].socket < 0) {
           echttp_raw_cleanup(i);
           return i;
       }
   }
   fprintf (stderr, "Too many client, reject this new one.\n");
   return -1;
}

static void echttp_raw_accept (echttp_raw_acceptor *acceptor, int server) {
   int i;
   struct sockaddr_in6 peer;
   socklen_t peerlength = sizeof(peer);

   int s = accept(server, (struct sockaddr *)(&peer), &peerlength);
   if (s < 0) {
       fprintf (stderr, "cannot accept new client: %s\n", strerror(errno));
       exit(1);
   }

   i = echttp_raw_new_client();
   if (i < 0) {
       close (s);
       return;
   }
   if (!acceptor(i)) {
       fprintf (stderr, "Client rejected.\n");
       close (s);
       return;
   }
   if (echttp_raw_debug) {
       if (peer.sin6_family == AF_INET6) {
           printf ("Accepting IPv6 client %d for port %d\n", i,
                   peer.sin6_port);
       } else if (peer.sin6_family == AF_INET) {
           printf ("Accepting IPv4 client %d\n", i); // Not expected.
       } else {
           printf ("Accepting client %d from unknown protocol\n", i);
       }
   }

   echttp_raw_client[i].socket = s;
   echttp_raw_client[i].peer = peer;
}

int echttp_raw_open (const char *service, int debug) {

   int i;

   struct sockaddr_in6 netaddress6;
   int port = -1;

   echttp_raw_debug = debug;

   for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
       echttp_raw_client[i].socket = -1;
   }

   if (strcmp ("dynamic", service) == 0) {
       port = 0;
   } else {
       struct servent *entry = getservbyname(service, "tcp");
       if (entry == NULL) {
           if (isdigit (service[0])) {
              port = atoi(service);
           }
       } else {
           port = ntohs(entry->s_port);
       }

       if (port <= 0) {
           fprintf (stderr, "invalid service name %s\n", service);
           return 0;
       }
       echttp_raw_serverport = port;
   }

   if (echttp_raw_debug) printf ("Opening server for port %d\n", port);

   echttp_raw_server = socket(PF_INET6, SOCK_STREAM, 0);
   if (echttp_raw_server < 0) {
       fprintf (stderr, "cannot open socket for service %s: %s\n",
                service, strerror(errno));
       return 0;
   }

   memset(&netaddress6, 0, sizeof(netaddress6));
   netaddress6.sin6_family = AF_INET6;
   netaddress6.sin6_addr = in6addr_any;
   netaddress6.sin6_port = htons(port);

   if (bind(echttp_raw_server,
            (struct sockaddr *)&netaddress6, sizeof(netaddress6)) < 0) {
       fprintf (stderr, "cannot bind to service %s: %s\n",
                service, strerror(errno));
       exit(0);
   }

   if (port == 0) {
       int addrlen = sizeof(netaddress6);
       getsockname (echttp_raw_server,
                    (struct sockaddr *)&netaddress6, &addrlen);
       port = echttp_raw_serverport = ntohs(netaddress6.sin6_port);
       if (echttp_raw_debug)
           printf ("Dynamic port allocated: %d\n", port);
   }

   if (listen (echttp_raw_server, 4) < 0) {
       fprintf (stderr, "listen to service %s failed: %s\n",
                service, strerror(errno));
       return 0;
   }

   return 1;
}

int echttp_raw_capacity (void) {
    return ECHTTP_CLIENT_MAX;
}

void echttp_raw_close_client (int i, const char *reason) {

    if (echttp_raw_client[i].socket >= 0) {
        if (echttp_raw_debug) printf ("closing client %d: %s\n", i, reason);
        close (echttp_raw_client[i].socket);
        if (echttp_raw_client[i].transfer.size > 0) {
            close (echttp_raw_client[i].transfer.fd);
        }
        echttp_raw_cleanup(i);
    }
}

static int echttp_raw_consume (echttp_buffer *buffer, int length) {
   buffer->start += length;
   if (buffer->start >= buffer->end) {
       buffer->start = buffer->end = 0;
   }
   return buffer->end == 0;
}

static void echttp_raw_transmit (int i) {

   echttp_buffer *buffer = &(echttp_raw_client[i].out);

   ssize_t length = buffer->end - buffer->start;
   if (length > 0) {

      if (length > ETH_MAX_FRAME) length = ETH_MAX_FRAME;

      length = send (echttp_raw_client[i].socket,
                     buffer->data + buffer->start, (size_t)length, 0);
      if (length <= 0) {
          echttp_raw_close_client (i, strerror(errno));
          return;
      }
      if (echttp_raw_debug) {
          printf ("Transmit data at offset %d: %*.*s\n",
                  buffer->start, 0-length, length, buffer->data + buffer->start);
      }
      if (echttp_raw_consume (buffer, length)) {
          if (echttp_raw_client[i].transfer.size <= 0) {
              echttp_raw_client[i].deadline = time(NULL) + 10;
          } else if (echttp_raw_debug) {
              printf ("Initiating field transfer (%d bytes)\n",
                      echttp_raw_client[i].transfer.size);
          }
      }

   } else if (echttp_raw_client[i].transfer.size > 0) {

       length = echttp_raw_client[i].transfer.size;
       if (length > ETH_MAX_FRAME) length = ETH_MAX_FRAME;

       length = sendfile (echttp_raw_client[i].socket,
                          echttp_raw_client[i].transfer.fd, 0, length);
       if (length <= 0) {
           echttp_raw_close_client (i, strerror(errno));
           return;
       }
       echttp_raw_client[i].transfer.size -= length;
       if (echttp_raw_client[i].transfer.size <= 0) {
           close (echttp_raw_client[i].transfer.fd);
           echttp_raw_client[i].transfer.fd = -1;
           echttp_raw_client[i].transfer.size = 0;

           echttp_raw_client[i].deadline = time(NULL) + 10;
       }
   }
}

static int echttp_split (char *data, const char *sep, char **items, int max) {
    int count = 0;
    int length = strlen(sep);
    char *start = data;

    while (*data) {
       if (strncmp(sep, data, length) == 0) {
           *data = 0;
           items[count++] = start;
           data += length;
           start = data;
       } else {
           data += 1;
       }
    }
    if (data > start) items[count++] = start;
    return count;
}

static void echttp_raw_receive (int i, echttp_raw_receiver received) {

   echttp_buffer *buffer = &(echttp_raw_client[i].in);

   ssize_t length = sizeof(buffer->data) - buffer->end - 1;
   if (length <= 0) {
       echttp_raw_close_client (i, "data too large");
       return;
   }
   length = recv (echttp_raw_client[i].socket,
                  buffer->data + buffer->end, (size_t)length-1, 0);
   if (length <= 0) {
       echttp_raw_close_client (i, strerror(errno));
       return;
   }
   buffer->end += length;
   buffer->data[buffer->end] = 0;

   if (echttp_raw_debug) {
       printf ("Data (client %d) = %s\n", i, buffer->data);
       fflush (stdout);
   }
   if (received) {
       length =
           received (i, buffer->data+buffer->start, buffer->end-buffer->start);
       if (echttp_raw_client[i].socket >= 0 && length > 0)
           echttp_raw_consume (buffer, length);
   }
}

static int echttp_raw_invalid (int client) {
   if ((client < 0) || (client >= ECHTTP_CLIENT_MAX)) {
       fprintf (stderr, "Invalid client number %d (out of range)\n", client);
       return 1;
   }
   if (echttp_raw_client[client].socket < 0) {
       fprintf (stderr, "Invalid client number %d (closed)\n", client);
       return 1;
   }
   return 0;
}

int  echttp_raw_is_local (int client) {

    if (echttp_raw_client[client].peer.sin6_family != AF_INET6) return 1;

    return 1; // TBD: adjust this check to IPv6 addresses.

    // echttp_raw_enumerate();
}

int  echttp_raw_server_port (int ip) {
    switch (ip) {
        case 4:
        case 6:
            return echttp_raw_serverport;
    }
    return 0; // Not a known IP version.
}

void echttp_raw_send (int client, const char *data, int length) {
   echttp_buffer *buffer = &(echttp_raw_client[client].out);
   if (echttp_raw_invalid(client)) return;
   if (length > sizeof(buffer->data) - buffer->end) {
       echttp_raw_close_client (client, "Transmit data is too large");
       return;
   }
   memcpy (buffer->data+buffer->end, data, length);
   buffer->end += length;
}

void echttp_raw_transfer (int client, int fd, int length) {
   if (echttp_raw_invalid(client)) return;
   echttp_raw_client[client].transfer.fd = fd;
   echttp_raw_client[client].transfer.size = length;
}

void echttp_raw_loop (echttp_raw_acceptor *accept,
                      echttp_raw_receiver *received) {

   struct timeval timeout;
   fd_set readset;
   fd_set writeset;
   int i;
   int count;
   int maxfd;

   while (echttp_raw_server >= 0) {
      FD_ZERO(&readset);
      FD_ZERO(&writeset);
      FD_SET(echttp_raw_server, &readset);
      maxfd = echttp_raw_server;

      for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
          int socket = echttp_raw_client[i].socket;
          if (socket >= 0) {
              if (echttp_raw_client[i].out.end > 0 ||
                  echttp_raw_client[i].transfer.size > 0) {
                  FD_SET(socket, &writeset);
              } else {
                  // Receive only after the previous response has been sent.
                  FD_SET(socket, &readset);
              }
              if (socket > maxfd) maxfd = socket;
          }
      }

      if (echttp_raw_backgrounder) {
          echttp_raw_backgrounder (0, 0);
      }

      for (i = 0; i < echttp_raw_other_count; ++i) {
          if (echttp_raw_other[i].mode) {
              int socket = echttp_raw_other[i].fd;
              if (echttp_raw_other[i].mode & 1) {
                  FD_SET(socket, &readset);
              }
              if (echttp_raw_other[i].mode & 2) {
                  FD_SET(socket, &writeset);
              }
              if (socket > maxfd) maxfd = socket;
          }
      }

      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      count = select(maxfd+1, &readset, &writeset, NULL, &timeout);

      if (count > 0) {
          for (i = 0; i < echttp_raw_other_count; ++i) {
              if (echttp_raw_other[i].premium == 0) continue;
              if (echttp_raw_other[i].mode == 0) continue;
              int fd = echttp_raw_other[i].fd;
              int mode = 0;
              if (FD_ISSET(fd, &readset)) mode |= 1;
              if (FD_ISSET(fd, &writeset)) mode |= 2;
              if (mode) {
                  echttp_raw_other[i].listener (fd, mode);
              }
          }

          if (FD_ISSET(echttp_raw_server, &readset)) {
              echttp_raw_accept(accept, echttp_raw_server);
          }

          for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
             int socket = echttp_raw_client[i].socket;
             if (socket >= 0) {
                 if (FD_ISSET(socket, &writeset)) {
                     echttp_raw_transmit (i);
                 }
                 if (FD_ISSET(socket, &readset)) {
                     echttp_raw_receive (i, received);
                 }
             }
          }
          for (i = 0; i < echttp_raw_other_count; ++i) {
              if (echttp_raw_other[i].mode == 0) continue;
              if (echttp_raw_other[i].premium) continue; // Don't call it twice.
              int fd = echttp_raw_other[i].fd;
              int mode = 0;
              if (FD_ISSET(fd, &readset)) mode |= 1;
              if (FD_ISSET(fd, &writeset)) mode |= 2;
              if (mode) {
                  echttp_raw_other[i].listener (fd, mode);
              }
          }
      }

      time_t now = time(NULL);
      for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
          if (echttp_raw_client[i].deadline == 0) continue;
          if (now > echttp_raw_client[i].deadline) {
              echttp_raw_close_client (i, "deadline reached");
          }
      }
   }
}

void echttp_raw_register (int fd, int mode,
                          echttp_listener *listener, int premium) {
    int i;
    if (listener == 0) mode = 0; // Disabled when no listener.

    // Is this an existing I/O to update?
    //
    for (i = 0; i < echttp_raw_other_count; ++i) {
        if (echttp_raw_other[i].fd == fd) { // Update existing entry.
            echttp_raw_other[i].mode = mode;
            if (mode) {
                echttp_raw_other[i].premium = premium;
                echttp_raw_other[i].listener = listener;
            }
            return;
        }
    }
    if (mode == 0) return; // Disabling a non-existent entry is no-op.

    // This is a new I/O to listen to.
    //
    if (echttp_raw_other_count >= ECHTTP_RAW_MAXREGISTERED) {
        fprintf (stderr, __FILE__ ": too many listeners\n");
        return;
    }
    echttp_raw_other[echttp_raw_other_count].mode = mode;
    echttp_raw_other[echttp_raw_other_count].premium = premium;
    echttp_raw_other[echttp_raw_other_count].listener = listener;
    echttp_raw_other[echttp_raw_other_count].fd = fd;
    echttp_raw_other_count += 1;
}

void echttp_raw_forget (int fd) {
    int i;
    for (i = 0; i < echttp_raw_other_count; ++i) {
        if (echttp_raw_other[i].fd == fd) {
            if (i < --echttp_raw_other_count) {
                echttp_raw_other[i] = echttp_raw_other[echttp_raw_other_count];
            }
            return;
        }
    }
}

void echttp_raw_background (echttp_listener *listener) {
    echttp_raw_backgrounder = listener;
}

int echttp_raw_connect (const char *host, const char *service) {

    int value;
    int s;

    static struct addrinfo hints;
    struct addrinfo *resolved;
    struct addrinfo *cursor;

    int i = echttp_raw_new_client ();
    if (i < 0) return i;

    hints.ai_flags = AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo (host, service, &hints, &resolved)) return -1;

    for (cursor = resolved; cursor; cursor = cursor->ai_next) {

        if (cursor->ai_family != AF_INET && cursor->ai_family != AF_INET6)
            continue;

        s = socket(cursor->ai_family, cursor->ai_socktype, cursor->ai_protocol);
        if (s < 0) continue;

        int flags = fcntl(s, F_GETFL);
        if (flags == -1) {
            close(s);
            continue;
        }
        flags |= O_NONBLOCK;
        fcntl(s, F_SETFL, flags);

        if (connect(s, cursor->ai_addr, cursor->ai_addrlen) != 0) {
            if (errno != EINPROGRESS) {
                if (echttp_raw_debug)
                    printf ("connection failed: %s\n", strerror(errno));
                close(s);
                continue;
            }
        }
        echttp_raw_client[i].socket = s;
        echttp_raw_client[i].peer = (struct sockaddr_in6){0};
        freeaddrinfo(resolved);
        return i;
    }
    freeaddrinfo(resolved);
    return -1;
}

void echttp_raw_close (void) {
   int i;
   for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
      if (echttp_raw_client[i].socket >= 0) {
          echttp_raw_close_client (i, "closing server");
      }
   }
   close(echttp_raw_server);
   echttp_raw_server = -1;
}

