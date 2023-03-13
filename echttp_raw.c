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
 * int echttp_raw_open (const char *service, int debug, int ttl);
 *
 *    Open the HTTP service socket, using the port defined by the service
 *    string (one need to remember that most echttp applciations do not
 *    use port 80). The ttl parameter defines how long echttp waits before
 *    killing an idle client connection.
 *
 * int echttp_raw_capacity (void);
 *
 *    Return the maximum number of concurrent clients supported.
 *    That number is typically the limit configured in the OS, but
 *    it might not be this way forever..
 *
 * int echttp_raw_server_port (int ip);
 *
 *    Return the current HTTP port being used. This might be a port
 *    number allocated by the OS within the dynamic range if the
 *    application specified the port number as 0.
 *
 *    The ip parameter is meant to represent the IP version (4 or 6).
 *    The profile of this function is likely to change in the future.
 *
 * int echttp_raw_send (int client, const char *data, int length);
 *
 *    Send raw data to the specified client. Note: this is used only for
 *    un-encrypted connections. See echttp_tls_send for encrypted connections.
 *
 * void echttp_raw_loop (echttp_raw_acceptor *acceptor,
 *                       echttp_raw_receiver *received,
 *                       echttp_raw_terminator *terminate);
 *
 *    This is the main event loop. It handles the server sockets, client
 *    sockets and any additional application file descriptor declared
 *    using echttp_raw_manage, echttp_raw_attach and echttp_raw_register.
 *
 *    The acceptor function is called when a new client was accepted. The
 *    received function is called with the data that was received (only if
 *    this module handle data exchange--i.e. HTPP remote and local clients).
 *    The terminate function is called before a client socket is closed.
 *
 * int echttp_raw_connect (const char *host, const char *service);
 *
 *    Create a new socket, connected to the specified server.
 *    This function does not cause echttp_raw to manage the connection:
 *    see echttp_raw_manage, echttp_raw_register and echttp_raw_update.
 *
 * int echttp_raw_manage   (int fd);
 *
 *    Declare a new HTTP local client. This is used when the application is
 *    the HTTP client, and not for remote clients connected to this server.
 *    The echttp_raw module will control this socket, i.e. handle data
 *    exchanges and decide when to close it, like for remote clients.
 *
 * int echttp_raw_attach   (int fd, int mode, echttp_listener *listener);
 *
 *    Attach a new TCP socket to echttp_raw. Data exchanges for this type of
 *    socket is handled by an external module. This is typically used for TLS
 *    connections. The echttp_raw module will still control when to close
 *    the socket.
 *
 * int echttp_raw_register (int fd, int mode,
 *                          echttp_listener *listener, int premium);
 *
 *    Attach a new file descriptor to echttp_raw. This is not necessarily
 *    a TCP socket, and echttp_raw will never close it. The data is handled
 *    by an external module. This is used for application I/O.
 *
 *    The premium parameter is a boolean that indicates if the file
 *    descriptor must be processed before all sockets, or else if it
 *    can be pocessed after.
 *
 * int echttp_raw_update   (int client, int mode);
 *
 *    Change the listening mode for the specified socket or file descriptor.
 *    The mode parameter can be either 1 (read only), 2 (write only) or
 *    3 (read and write). This defines what echttp_raw will listen for.
 *
 * void echttp_raw_close_client (int i, const char *reason);
 *
 *    Close a client socket, either a local or remote client.
 *
 * void echttp_raw_forget (int fd);
 *
 *    Remove a file descriptor from the pool that echttp_raw listens to.
 *    The application module must close the file descriptor on its own.
 *
 * void echttp_raw_close (void);
 *
 *    Close the complete HTTP service. This also closes all local and remote
 *    client sockets.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/resource.h>
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
static int echttp_raw_ttl = 10;


#define ECHTTP_RAW_UNUSED 0
#define ECHTTP_RAW_TCP    1 // Total control of connection and data.
#define ECHTTP_RAW_APP    2 // Control of connection, not of data.
#define ECHTTP_RAW_LISTEN 3 // Unknown resource, no control

typedef struct {
    int mode;
    echttp_listener *listener;
} echttp_raw_listen;

#define ECHTTP_CLIENT_BUFFER 102400
typedef struct {
    char data[ECHTTP_CLIENT_BUFFER];
    int start;
    int end;
} echttp_buffer;

typedef struct {
    struct sockaddr_in6 peer;
    echttp_buffer in;
    echttp_buffer out;
    struct {
        int fd;
        int size;
    } transfer;
} echttp_raw_tcp;

typedef struct {
    int fd;
    char use;
    char premium;
    time_t deadline;
    union {
        echttp_raw_listen listen; // Also used by ECHTTP_RAW_APP.
        echttp_raw_tcp    tcp;
    } *data;
} echttp_raw_context;

static echttp_raw_context *echttp_raw_io = 0;
static int echttp_raw_io_size = 0;
static int echttp_raw_io_last = 0;

static echttp_listener *echttp_raw_backgrounder = 0;

static int echttp_raw_serverport = 0;

static echttp_raw_terminator *echttp_raw_terminate = 0;


#define ECHTTP_IF_MAX 16
static struct {
    uint32_t ifaddr;
    uint32_t ifmask;
} echttp_raw_if[ECHTTP_IF_MAX];

static int echttp_raw_ifcount = 0;
static time_t echttp_raw_timestamp = 0;


static void echttp_raw_io_cleanup (int i) {
   echttp_raw_io[i].fd = -1;
   echttp_raw_io[i].use = ECHTTP_RAW_UNUSED;
   echttp_raw_io[i].premium = 0;
   echttp_raw_io[i].deadline = 0;
   if (i == echttp_raw_io_last) {
       while (echttp_raw_io[--echttp_raw_io_last].use == ECHTTP_RAW_UNUSED) ;
   }
}

static int echttp_raw_io_new (char use, int fd) {

   static char *toname[4] = {"unused", "tcp", "app", "listen"};
   int i;

   if (use == ECHTTP_RAW_UNUSED) return -1;

   for (i = 0; i < echttp_raw_io_size; ++i) {
       if (echttp_raw_io[i].use == ECHTTP_RAW_UNUSED) {

           echttp_raw_io[i].fd = fd;
           echttp_raw_io[i].use = use;
           echttp_raw_io[i].deadline = 0;
           echttp_raw_io[i].premium = 0;

           if (!echttp_raw_io[i].data)
               echttp_raw_io[i].data =
                   malloc (sizeof(*(echttp_raw_io[i].data)));

           switch (use) {
               case ECHTTP_RAW_TCP:
                   echttp_raw_io[i].data->tcp.in.start = 0;
                   echttp_raw_io[i].data->tcp.in.end = 0;
                   echttp_raw_io[i].data->tcp.out.start = 0;
                   echttp_raw_io[i].data->tcp.out.end = 0;
                   echttp_raw_io[i].data->tcp.transfer.fd = -1;
                   echttp_raw_io[i].data->tcp.transfer.size = 0;
                   break;
               case ECHTTP_RAW_APP:
               case ECHTTP_RAW_LISTEN:
                   echttp_raw_io[i].data->listen.mode = 0;
                   echttp_raw_io[i].data->listen.listener = 0;
                   break;
           }

           if (i > echttp_raw_io_last) echttp_raw_io_last = i;
           if (echttp_raw_debug)
               printf ("New client %d, socket %d (%s)\n", i, fd, toname[use]);
           return i;
       }
   }
   fprintf (stderr, "Too many IO, reject this new one.\n");
   return -1;
}

static const char *echttp_printip (long ip) {
    static char ascii[16];

    snprintf (ascii, sizeof(ascii), "%ld.%ld.%ld.%ld",
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

static void echttp_raw_accept (echttp_raw_acceptor *acceptor, int server) {
   int i;
   struct sockaddr_in6 peer;
   socklen_t peerlength = sizeof(peer);

   int s = accept(server, (struct sockaddr *)(&peer), &peerlength);
   if (s < 0) {
       fprintf (stderr, "cannot accept new client: %s\n", strerror(errno));
       exit(1);
   }
   if (echttp_raw_debug) printf ("Accepting socket %d\n", s);

   i = echttp_raw_io_new(ECHTTP_RAW_TCP, s);
   if (i < 0) {
       if (echttp_raw_debug)
           printf ("No client slot to accept socket %d\n", s);
       close (s);
       return;
   }
   if (!acceptor(i)) {
       fprintf (stderr, "Client rejected.\n");
       echttp_raw_io_cleanup(i);
       close (s);
       return;
   }
   if (echttp_raw_debug) {
       if (peer.sin6_family == AF_INET6) {
           printf ("Client %d accepted IPv6 from port %d\n",
                   i, ntohs(peer.sin6_port));
       } else if (peer.sin6_family == AF_INET) {
           printf ("Client %d accepted IPv4\n", i); // Not expected.
       } else {
           printf ("Client %d accepted unknown protocol\n", i);
       }
   }

   echttp_raw_io[i].deadline = time(0) + echttp_raw_ttl;
   echttp_raw_io[i].data->tcp.peer = peer;
}

int echttp_raw_open (const char *service, int debug, int ttl) {

   int i;

   struct sockaddr_in6 netaddress6;
   int port = -1;
   struct rlimit limit;

   echttp_raw_debug = debug;
   if (ttl > 0) echttp_raw_ttl = ttl;

   getrlimit(RLIMIT_NOFILE, &limit);
   echttp_raw_io_size = limit.rlim_cur;
   echttp_raw_io = calloc (echttp_raw_io_size, sizeof(*echttp_raw_io));

   for (i = 0; i < echttp_raw_io_size; ++i) {
       echttp_raw_io[i].use = ECHTTP_RAW_UNUSED;
       echttp_raw_io[i].fd = -1;
       echttp_raw_io[i].premium = 0;
       echttp_raw_io[i].deadline = 0;
       echttp_raw_io[i].data = 0;
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

       if (port <= 0 || port >= 0x10000) {
           fprintf (stderr, "invalid service name  or number %s\n", service);
           return 0;
       }
       echttp_raw_serverport = port;
   }

   if (echttp_raw_debug) printf ("Opening server for port %d\n", port);

   signal(SIGPIPE, SIG_IGN);

   echttp_raw_server = socket(PF_INET6, SOCK_STREAM, 0);
   if (echttp_raw_server < 0) {
       fprintf (stderr, "cannot open socket for service %s: %s\n",
                service, strerror(errno));
       return 0;
   }
   int option = 1;
   setsockopt(echttp_raw_server,
              SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));

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
    return echttp_raw_io_size;
}

void echttp_raw_close_client (int i, const char *reason) {

    if (echttp_raw_io[i].fd >= 0) {
        if (echttp_raw_debug) printf ("Client %d closing: %s\n", i, reason);
        switch (echttp_raw_io[i].use) {
            case ECHTTP_RAW_TCP:
                if (echttp_raw_io[i].data->tcp.transfer.size > 0) {
                    close (echttp_raw_io[i].data->tcp.transfer.fd);
                }
            case ECHTTP_RAW_APP:
                if (echttp_raw_terminate)
                    echttp_raw_terminate (i, reason);
                close (echttp_raw_io[i].fd);
                break;
        }
        echttp_raw_io_cleanup(i);
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

   if (echttp_raw_io[i].use != ECHTTP_RAW_TCP) return;

   echttp_buffer *buffer = &(echttp_raw_io[i].data->tcp.out);

   ssize_t length = buffer->end - buffer->start;
   if (length > 0) {

      if (length > ETH_MAX_FRAME) length = ETH_MAX_FRAME;

      length = send (echttp_raw_io[i].fd,
                     buffer->data + buffer->start, (size_t)length, 0);
      if (length <= 0) {
          if (echttp_raw_debug) printf ("Client %d: send() error\n", i);
          echttp_raw_close_client (i, strerror(errno));
          return;
      }
      if (echttp_raw_debug) {
          printf ("Transmit data at offset %d: %*.*s\n",
                  buffer->start, (int)(0-length), (int)length, buffer->data + buffer->start);
      }
      if (echttp_raw_consume (buffer, length)) {
          if (echttp_raw_io[i].data->tcp.transfer.size <= 0) {
              echttp_raw_io[i].deadline = time(0) + echttp_raw_ttl;
          } else if (echttp_raw_debug) {
              printf ("Initiating field transfer (%d bytes)\n",
                      echttp_raw_io[i].data->tcp.transfer.size);
          }
      }

   } else if (echttp_raw_io[i].data->tcp.transfer.size > 0) {

       length = echttp_raw_io[i].data->tcp.transfer.size;
       if (length > ETH_MAX_FRAME) length = ETH_MAX_FRAME;

       length = sendfile (echttp_raw_io[i].fd,
                          echttp_raw_io[i].data->tcp.transfer.fd, 0, length);
       if (length <= 0) {
           echttp_raw_close_client (i, strerror(errno));
           return;
       }
       echttp_raw_io[i].data->tcp.transfer.size -= length;
       if (echttp_raw_io[i].data->tcp.transfer.size <= 0) {
           close (echttp_raw_io[i].data->tcp.transfer.fd);
           echttp_raw_io[i].data->tcp.transfer.fd = -1;
           echttp_raw_io[i].data->tcp.transfer.size = 0;

           echttp_raw_io[i].deadline = time(0) + echttp_raw_ttl;
       }
   }
}

static void echttp_raw_receive (int i, echttp_raw_receiver received) {

   echttp_buffer *buffer = &(echttp_raw_io[i].data->tcp.in);

   ssize_t length = sizeof(buffer->data) - buffer->end - 1;
   if (length <= 0) {
       if (received) received (i, 0, -1);
       echttp_raw_close_client (i, "data too large");
       return;
   }
   length = recv (echttp_raw_io[i].fd,
                  buffer->data + buffer->end, (size_t)length-1, 0);
   if (length <= 0) {
       if (echttp_raw_debug)
           printf ("Client %d recv() error on socket %d\n", i, echttp_raw_io[i].fd);
       if (received) received (i, 0, -1);
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
       if (echttp_raw_io[i].fd >= 0 && length > 0)
           echttp_raw_consume (buffer, length);
   }
}

static int echttp_raw_invalid (int client) {
   if ((client < 0) || (client > echttp_raw_io_last)) {
       fprintf (stderr, "Invalid client number %d (out of range)\n", client);
       return 1;
   }
   if (echttp_raw_io[client].fd < 0) {
       fprintf (stderr, "Invalid client number %d (closed)\n", client);
       return 1;
   }
   return 0;
}

int  echttp_raw_is_local (int client) {

    if (echttp_raw_io[client].data->tcp.peer.sin6_family != AF_INET6) return 1;

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

   if (echttp_raw_io[client].use != ECHTTP_RAW_TCP) return;

   echttp_buffer *buffer = &(echttp_raw_io[client].data->tcp.out);
   if (echttp_raw_invalid(client)) return;
   if (length > sizeof(buffer->data) - buffer->end) {
       echttp_raw_close_client (client, "Transmit data is too large");
       return;
   }
   memcpy (buffer->data+buffer->end, data, length);
   buffer->end += length;
}

void echttp_raw_transfer (int client, int fd, int length) {

   if (echttp_raw_io[client].use != ECHTTP_RAW_TCP) return;

   if (echttp_raw_invalid(client)) return;
   echttp_raw_io[client].data->tcp.transfer.fd = fd;
   echttp_raw_io[client].data->tcp.transfer.size = length;
}

void echttp_raw_loop (echttp_raw_acceptor *accept,
                      echttp_raw_receiver *received,
                      echttp_raw_terminator *terminate) {

   struct timeval timeout;
   fd_set readset;
   fd_set writeset;
   int i;
   int count;

   echttp_raw_terminate = terminate;

   while (echttp_raw_server >= 0) {
      int maxfd = echttp_raw_server;
      FD_ZERO(&readset);
      FD_ZERO(&writeset);
      FD_SET(echttp_raw_server, &readset);

      for (i = 0; i <= echttp_raw_io_last; ++i) {
          int mode;
          int fd = echttp_raw_io[i].fd;
          switch (echttp_raw_io[i].use) {
              case ECHTTP_RAW_TCP:
                  if (echttp_raw_io[i].data->tcp.out.end > 0 ||
                      echttp_raw_io[i].data->tcp.transfer.size > 0) {
                      FD_SET(fd, &writeset);
                  } else {
                      // Receive only after the previous response has been sent.
                      FD_SET(fd, &readset);
                  }
                  break;
              case ECHTTP_RAW_APP:
              case ECHTTP_RAW_LISTEN:
                  mode = echttp_raw_io[i].data->listen.mode;
                  if (!mode) continue;
                  if (mode & 1) {
                      FD_SET(fd, &readset);
                  }
                  if (mode & 2) {
                      FD_SET(fd, &writeset);
                  }
                  break;
              default:
                  continue;
          }
          if (fd > maxfd) maxfd = fd;
      }

      if (echttp_raw_backgrounder) {
          echttp_raw_backgrounder (0, 0);
      }

      timeout.tv_sec = 1;
      timeout.tv_usec = 0;
      count = select(maxfd+1, &readset, &writeset, NULL, &timeout);

      time_t now = time(0);

      if (count > 0) {
          for (i = 0; i <= echttp_raw_io_last; ++i) {
              if (!echttp_raw_io[i].premium) continue;
              if (echttp_raw_io[i].use != ECHTTP_RAW_LISTEN) continue;
              if (!echttp_raw_io[i].data->listen.mode) continue;
              int fd = echttp_raw_io[i].fd;
              int mode = 0;
              if (FD_ISSET(fd, &readset)) mode |= 1;
              if (FD_ISSET(fd, &writeset)) mode |= 2;
              if (mode) {
                  echttp_raw_io[i].data->listen.listener (fd, mode);
              }
          }

          for (i = 0; i <= echttp_raw_io_last; ++i) {
             int fd = echttp_raw_io[i].fd;
             switch (echttp_raw_io[i].use) {
                 case ECHTTP_RAW_TCP:
                     if (FD_ISSET(fd, &writeset)) {
                         echttp_raw_transmit (i);
                     }
                     if (FD_ISSET(fd, &readset)) {
                         echttp_raw_receive (i, received);
                     }
                     break;
                 case ECHTTP_RAW_LISTEN:
                     if (echttp_raw_io[i].premium) continue;
                 case ECHTTP_RAW_APP:
                     if (!echttp_raw_io[i].data->listen.mode) continue;
                     int mode = 0;
                     if (FD_ISSET(fd, &readset)) mode |= 1;
                     if (FD_ISSET(fd, &writeset)) mode |= 2;
                     if (mode) {
                         int id = fd;
                         if (echttp_raw_io[i].use == ECHTTP_RAW_APP) {
                             echttp_raw_io[i].deadline = now + echttp_raw_ttl;
                             id = i;
                         }
                         echttp_raw_io[i].data->listen.listener (id, mode);
                     }
                     break;
                 default:
                     break;
             }
          }

          if (FD_ISSET(echttp_raw_server, &readset)) {
              echttp_raw_accept(accept, echttp_raw_server);
          }
      }

      for (i = 0; i <= echttp_raw_io_last; ++i) {
          if (echttp_raw_io[i].deadline == 0) continue;
          if (now > echttp_raw_io[i].deadline) {
              echttp_raw_close_client (i, "deadline reached");
          }
      }
   }
}

int echttp_raw_register (int fd, int mode,
                         echttp_listener *listener, int premium) {
    int i;
    int use = ECHTTP_RAW_LISTEN;
    if (listener == 0) mode = 0; // Disabled when no listener.
    if (premium < 0) {
        use = ECHTTP_RAW_APP;
        premium = 0;
    }
    if (echttp_raw_debug)
        printf ("Registering socket %d, mode %d premium %d\n", fd, mode, premium);

    // Is this an existing I/O to update?
    //
    for (i = 0; i <= echttp_raw_io_last; ++i) {
        if (echttp_raw_io[i].fd == fd) { // Update existing entry.
            if (echttp_raw_io[i].use != use) return -1;
            echttp_raw_io[i].data->listen.mode = mode;
            if (mode) {
                echttp_raw_io[i].premium = premium;
                echttp_raw_io[i].data->listen.listener = listener;
            }
            return i;
        }
    }
    if (mode == 0) return -1; // Disabling a non-existent entry is no-op.

    // This is a new I/O to listen to.
    //
    i = echttp_raw_io_new (use, fd);
    if (i < 0) {
        fprintf (stderr, __FILE__ ": too many listeners\n");
        return -1;
    }
    echttp_raw_io[i].premium = premium;
    echttp_raw_io[i].data->listen.mode = mode;
    echttp_raw_io[i].data->listen.listener = listener;
    if (echttp_raw_debug)
        printf ("Client %d registered socket %d (%s)\n",
                i, fd, (use == ECHTTP_RAW_LISTEN)?"listen":"app");
    return i;
}

int echttp_raw_attach (int fd, int mode, echttp_listener *listener) {
    return echttp_raw_register (fd, mode, listener, -1);
}

int echttp_raw_update (int client, int mode) {

    if (client < 0 || client > echttp_raw_io_last) return -1;

    switch (echttp_raw_io[client].use) {
        case ECHTTP_RAW_APP:
        case ECHTTP_RAW_LISTEN:
            echttp_raw_io[client].data->listen.mode = mode;
            break;
        default:
            break;
    }
    return client;
}

void echttp_raw_forget (int fd) {
    int i;
    for (i = 0; i < echttp_raw_io_last; ++i) {
        if (echttp_raw_io[i].fd == fd) {
            if (echttp_raw_io[i].use == ECHTTP_RAW_LISTEN)
                echttp_raw_io_cleanup (i);
            return;
        }
    }
}

void echttp_raw_background (echttp_listener *listener) {
    echttp_raw_backgrounder = listener;
}

int echttp_raw_connect (const char *host, const char *service) {

    int s = -1;

    static struct addrinfo hints;
    struct addrinfo *resolved;
    struct addrinfo *cursor;

    if (echttp_raw_debug) printf ("Connecting to %s:%s\n", host, service);
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
            s = -1;
            continue;
        }
        flags |= O_NONBLOCK;
        fcntl(s, F_SETFL, flags);

        if (connect(s, cursor->ai_addr, cursor->ai_addrlen) != 0) {
            if (errno != EINPROGRESS) {
                if (echttp_raw_debug)
                    printf ("connection failed: %s\n", strerror(errno));
                close(s);
                s = -1;
                continue;
            }
        }
        break; // Since we got a proper socket, no need to continue.
    }
    freeaddrinfo(resolved);
    return s;
}

int echttp_raw_manage (int s) {

    if (s < 0) return -1;

    if (echttp_raw_debug) printf ("Managing socket %d\n", s);
    int i = echttp_raw_io_new (ECHTTP_RAW_TCP, s);
    if (i < 0) {
        if (echttp_raw_debug)
            printf ("No client slot for managing socket %d\n", s);
        close(s);
        return -1;
    }
    if (echttp_raw_debug)
        printf ("Client %d managing socket %d\n", i, s);
    echttp_raw_io[i].data->tcp.peer = (struct sockaddr_in6){0};
    return i;
}

void echttp_raw_close (void) {
   int i;
   for (i = 0; i <= echttp_raw_io_last; ++i) {
       if (echttp_raw_io[i].fd >= 0) {
           echttp_raw_close_client (i, "closing server");
       }
   }
   close(echttp_raw_server);
   echttp_raw_server = -1;
}

