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
 * typedef int echttp_raw_callback (int client, char *data, int length);
 * int echttp_raw_open (const char *service, int debug);
 *
 * int echttp_raw_server_port (int ip);
 *
 * int echttp_raw_send (int client, const char *data, int length, int hangup);
 *
 * void echttp_raw_loop (echttp_raw_callback *received);
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

#define ECHTTP_CLIENT_MAX 16
static struct {
    int socket;
    struct sockaddr_in peer;
    int hangup;
    time_t deadline;
    echttp_buffer in;
    echttp_buffer out;
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
   echttp_raw_client[i].hangup = 0;
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

static void echttp_raw_accept (void) {
   int i;
   struct sockaddr_in peer;
   socklen_t peerlength = sizeof(peer);

   int client = accept(echttp_raw_server,
                       (struct sockaddr *)(&peer), &peerlength);
   if (client < 0) {
       fprintf (stderr, "cannot accept new client: %s\n", strerror(errno));
       return;
   }

   for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
       if (echttp_raw_client[i].socket < 0) {
           if (echttp_raw_debug) {
               printf ("Accepting client %d from %s:%d\n", i,
                       echttp_printip(ntohl((long)(peer.sin_addr.s_addr))),
                       peer.sin_port);
           }
           echttp_raw_cleanup(i);
           echttp_raw_client[i].socket = client;
           echttp_raw_client[i].peer = peer;
           return;
       }
   }

   fprintf (stderr, "Too many client, reject this new one.\n");
   close (client);
}

int echttp_raw_open (const char *service, int debug) {

   int i;

   struct sockaddr_in netaddress;
   int port = -1;

   echttp_raw_debug = debug;

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

   echttp_raw_server = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
   if (echttp_raw_server < 0) {
       fprintf (stderr, "cannot open socket for service %s: %s\n",
                service, strerror(errno));
       return 0;
   }

   memset(&netaddress, 0, sizeof(netaddress));
   netaddress.sin_family = AF_INET;
   netaddress.sin_addr.s_addr = INADDR_ANY;
   netaddress.sin_port = htons(port);

   if (bind(echttp_raw_server,
            (struct sockaddr *)&netaddress, sizeof(netaddress)) < 0) {
       fprintf (stderr, "cannot bind to service %s: %s\n",
                service, strerror(errno));
       return 0;
   }

   if (port == 0) {
       int addrlen = sizeof(netaddress);
       getsockname (echttp_raw_server,
                    (struct sockaddr *)&netaddress, &addrlen);
       echttp_raw_serverport = ntohs(netaddress.sin_port);
       if (echttp_raw_debug)
           printf ("Dynamic port allocated: %d\n", echttp_raw_serverport);
   }

   if (listen (echttp_raw_server, 4) < 0) {
       fprintf (stderr, "listen to service %s failed: %s\n",
                service, strerror(errno));
       return 0;
   }

   for (i = 0; i < ECHTTP_CLIENT_MAX; ++i) {
       echttp_raw_client[i].socket = -1;
   }
   return 1;
}

static void echttp_raw_close_client (int i, const char *reason) {
    if (echttp_raw_client[i].socket >= 0) {
        if (echttp_raw_debug) printf ("closing client %d: %s\n", i, reason);
        close (echttp_raw_client[i].socket);
        echttp_raw_cleanup(i);
    }
}

static int echttp_raw_consume (echttp_buffer *buffer, int length) {
   buffer->start += length;
   if (buffer->start >= buffer->end) {
       buffer->start = buffer->end = 0;
   }
   return buffer->start == 0;
}

static void echttp_raw_transmit (int i) {

   echttp_buffer *buffer = &(echttp_raw_client[i].out);

   ssize_t length = buffer->end - buffer->start;
   if (length > ETH_MAX_FRAME) length = ETH_MAX_FRAME;

   length = send (echttp_raw_client[i].socket,
                  buffer->data + buffer->start, (size_t)length, 0);
   if (length <= 0) {
       echttp_raw_close_client (i, strerror(errno));
       return;
   }
   if (echttp_raw_debug) {
       printf ("Transmit data at %d: %*.*s\n",
               buffer->start, 0-length, length, buffer->data + buffer->start);
   }
   if (echttp_raw_consume (buffer, length)) {
       echttp_raw_client[i].deadline = time(NULL) + 10;
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

static void echttp_raw_receive (int i, echttp_raw_callback received) {

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
       if (length > 0) echttp_raw_consume (buffer, length);
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

    int i;
    uint32_t ipaddr = echttp_raw_client[client].peer.sin_addr.s_addr;

    echttp_raw_enumerate();

    if (echttp_raw_debug) {
        printf ("Comparing %s to:\n", echttp_printip (ntohl((long)ipaddr)));
    }
    for (i = 0; i < echttp_raw_ifcount; ++i) {
        uint32_t ifmask = echttp_raw_if[i].ifmask;
        if (echttp_raw_debug) {
            printf ("   %s\n",
                    echttp_printip (ntohl((long)echttp_raw_if[i].ifaddr)));
        }
        if ((echttp_raw_if[i].ifaddr & ifmask) == (ipaddr & ifmask))
            return 1;
    }
    return 0;
}

int  echttp_raw_server_port (int ip) {
    switch (ip) {
        case 4: return echttp_raw_serverport;
        case 6: return 0; // No IPv6 support for now.
    }
    return 0; // Not a known IP version.
}

void echttp_raw_send (int client, const char *data, int length, int hangup) {
   echttp_buffer *buffer = &(echttp_raw_client[client].out);
   if (echttp_raw_invalid(client)) return;
   if (length > sizeof(buffer->data) - buffer->end) {
       echttp_raw_close_client (client, "Transmit data is too large");
       return;
   }
   memcpy (buffer->data+buffer->end, data, length);
   buffer->end += length;
   echttp_raw_client[client].hangup |= hangup;
}

void echttp_raw_loop (echttp_raw_callback *received) {

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
              if (echttp_raw_client[i].out.end > 0) {
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
              echttp_raw_accept();
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
    if (listener == 0) mode = 0; // Ignore when no listener.
    for (i = 0; i < echttp_raw_other_count; ++i) {
        if (echttp_raw_other[i].fd == fd) {
            echttp_raw_other[i].mode = mode;
            if (mode) {
                echttp_raw_other[i].listener = listener;
            } else {
                if (i == echttp_raw_other_count-1) echttp_raw_other_count -= 1;
            }
            return;
        }
    }
    if (mode == 0) return;

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

void echttp_raw_background (echttp_listener *listener) {
    echttp_raw_backgrounder = listener;
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

