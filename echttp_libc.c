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
 *
 *    This implementation tries to conform to all known variants, like how
 *    to signal truncation, in the safest possible manner. It also adds
 *    a non-standard protection against null source, because this is common
 *    with echttp (like missing optional HTTP argument or attribute).
 *
 * ssize_t strtcpy (char *dst, const char *src, size_t dsize);
 *
 *    Provide a safe and efficient function for string copy.
 *    See man strtcpy for more information.
 */

#include <string.h>

#include "echttp_libc.h"

char *stpecpy (char *dst, char *end, const char *restrict src) {

   if (dst == 0) return 0; // Protect against some variant implementations.
   if (dst >= end) return end;

   // There is room for at least one byte..
   if (src == 0) { // Add some custom protection.
      dst[0] = 0;
      return dst; // Nothing copied.
   }
   char *p = memccpy(dst, src, '\0', end - dst);
   if (p) return p - 1;

   /* truncation detected */
   end[-1] = '\0';
   return end;
}

ssize_t strtcpy (char *dst, const char *src, size_t dsize) {

   if (dst == 0) return -1;
   if (dsize <= 0) return -1;

   // There is room for at least one byte..
   if (src == 0) {
      dst[0] = 0;
      return -1; // Nothing copied.
   }
   char *p = memccpy(dst, src, '\0', dsize);
   if (p) return (ssize_t)(p - dst) - 1;

   dst[dsize-1] = 0;
   return -1;
}

