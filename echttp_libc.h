/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * applications.
 *
 * Declare semi-standard functions that should probably be part
 * of the C library in the first place. The intent is to eliminate
 * these local implementations when they are part of a standard library.
 */

#ifndef ECHTTP_LIBC_H__INCLUDED
#define ECHTTP_LIBC_H__INCLUDED

#include <sys/types.h>

char *stpecpy (char *dest, char *end, const char *restrict src);
ssize_t strtcpy (char *dst, const char *src, size_t dsize);
#endif

