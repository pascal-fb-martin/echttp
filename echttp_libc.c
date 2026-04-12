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
 *
 * char *stpedec (char *dest, char *end, long long value);
 *
 *    This function is made-up, not from any standard or commonly known
 *    implementation. The goal is speed.
 *
 *    This converts an integer to a decimal string. It is otherwise similar
 *    to, and is designed to work in combination with, stpecpy().
 */

#include <string.h>

#include "echttp_libc.h"

char *stpecpy (char *dst, char *end, const char *restrict src) {

   if (dst == 0) return 0;
   if (dst >= end) return 0; // Protect against some variant implementations.

   // There is room for at least one byte..
   if (src == 0) { // Add some custom protection.
      dst[0] = 0;
      return 0; // Nothing copied.
   }
   char *p = memccpy(dst, src, '\0', end - dst);
   if (p) return p - 1;

   /* truncation detected */
   end[-1] = '\0';
   return 0;
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

// Format two digits backward.
// This assumes that the buffer is large enough, but this is always
// the case for this static function.
static char *stpbdecstep (char *begin, char *end, long long val) {

    static const char *ascii[100] = {
        "00", "01", "02", "03", "04", "05", "06", "07", "08", "09",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99"
    };

    const char *a = ascii[val % 100];
    *(--end) = a[1];
    *(--end) = a[0];
    return end;
}

char *stpedec (char *dst, char *end, long long val) {

    static const char *headascii[100] = {
        "0",  "1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9",
        "10", "11", "12", "13", "14", "15", "16", "17", "18", "19",
        "20", "21", "22", "23", "24", "25", "26", "27", "28", "29",
        "30", "31", "32", "33", "34", "35", "36", "37", "38", "39",
        "40", "41", "42", "43", "44", "45", "46", "47", "48", "49",
        "50", "51", "52", "53", "54", "55", "56", "57", "58", "59",
        "60", "61", "62", "63", "64", "65", "66", "67", "68", "69",
        "70", "71", "72", "73", "74", "75", "76", "77", "78", "79",
        "80", "81", "82", "83", "84", "85", "86", "87", "88", "89",
        "90", "91", "92", "93", "94", "95", "96", "97", "98", "99"
    };

    if (dst == 0) return 0;
    if (dst >= end) return 0;

    const char *a;
    char *last = end - 1;

    if (val < 0) {
        if (dst >= last) goto truncate;
        *(dst++) = '-';
        *dst = 0;
        val = 0 - val;
    }

    // Optimize a very common case.
    if (val < 100) {
        if (dst >= last) goto truncate;
        a = headascii[val];
        *(dst++) = a[0];
        if (a[1]) {
           if (dst >= last) goto truncate;
           *(dst++) = a[1];
        }
        *dst = 0;
        return dst;
    }

    // The value is large enough that a local buffer is needed.
    //
    char image[32]; // Buffer is enough for any long long value.
    char *imagestep = image + sizeof(image);

    image[0] = 0;
    *(--imagestep) = 0;
    do {
        imagestep = stpbdecstep (image, imagestep, val);
        val /= 100;
    } while (val >= 100);

    a = headascii[val];
    if (dst >= last) goto truncate;
    *(dst++) = a[0];
    if (a[1]) {
       if (dst >= last) goto truncate;
       *(dst++) = a[1];
    }
    return stpecpy (dst, end, imagestep);

truncate:
    *dst = 0;
    return 0;
}

