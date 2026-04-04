/* echttp - Embedded HTTP server.
 *
 * This code is in the public domain.
 * ------------------------------------------------------------------------
 *
 * This module provides semi-standard functions that should probably be part
 * of the C library in the first place. The intent is to eliminate these
 * local implementations when these are part of a standard library.
 *
 * char *stpecpy (char *dest, char *end, const char *restrict src);
 *
 *    Provide a safe and efficient function for string copy.
 *    See man stpecpy for more information.
 */

#include <string.h>

#include "echttp_libc.h"

char *stpecpy (char *dst, char *end, const char *restrict src) {

   if (dst == 0) return 0; // Protect against some variant implementations.
   if (dst == end) return end;
   char *p = memccpy(dst, src, '\0', end - dst);
   if (p) return p - 1;

   /* truncation detected */
   end[-1] = '\0';
   return end;
}

