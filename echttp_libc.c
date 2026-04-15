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
#include <stdint.h>

#include "echttp_libc.h"

char *stpecpy (char *dst, char *end, const char *restrict src) {

   if (dst == 0) return 0;
   if (dst >= end) return 0; // Protect against some variant implementations.

   // There is room for at least one byte..
   if (src == 0) { // Add some custom protection.
      dst[0] = 0;
      return dst; // Nothing copied.
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

char *stpedec (char *dst, char *end, long long val) {

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

    if (dst == 0) return 0;
    if (dst >= end) return 0;

    const char *a;
    char *last = end - 1;

    if (val < 0) {
        // Eliminate a tricky case: abs(INT64_MIN) > INT64_MAX.
        if (val == INT64_MIN) {
           return stpecpy (dst, end, "-9223372036854775808");
        }
        if (dst >= last) goto truncate;
        *(dst++) = '-';
        val = 0 - val;
    }

    // Optimize the most common cases.
    if (val < 100) {
        if (val < 10) {
            if (dst >= last) goto truncate;
            *(dst++) = '0' + val;
            *dst = 0;
            return dst;
        }
        a = ascii[val];
        if (dst >= last-1) {
            if (dst >= last) goto truncate;
            *(dst++) = a[0]; // One slot left: do an exact truncation.
            goto truncate;
        }
        *(dst++) = a[0];
        *(dst++) = a[1];
        *dst = 0;
        return dst;
    }

    static long long IntegerLengths [] = {
        0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000,
        1000000000, 10000000000, 100000000000, 1000000000000, 10000000000000,
        100000000000000, 1000000000000000, 10000000000000000,
        100000000000000000, 1000000000000000000};

    // Calculate the length of the ASCII string to be generated.
    // This uses a binary search to optimize the loop for large numbers
    // (this also makes the loop worse for small numbers, but numbers
    // from 0 to 99 were excluded already--see above).
    int min = 2; // We already eliminated the case "val < 100".
    int max = 19;
    do {
        int mid = (min + max) / 2;
        if (val < IntegerLengths[mid]) max = mid;
        else min = mid;
    } while (max - min > 1);

    // Check that it fits, adjust to make it fit otherwise (truncation).
    //
    int truncated = 0;
    char *actualend = dst + min + 1; // Need room for the null terminator.
    if (actualend > last) {
        // Truncation will occur: adjust val to fit.
        val /= IntegerLengths[actualend - last];
        actualend = last;
        truncated = 1;
    }
    *actualend = 0;
    char *cursor = actualend;

    while (val >= 100) {
        const char *a = ascii[val % 100];
        val /= 100;
        *(--cursor) = a[1];
        *(--cursor) = a[0];
    }
    if (val < 10) {
        *(--cursor) = '0' + val;
    } else {
        const char *a = ascii[val];
        *(--cursor) = a[1];
        *(--cursor) = a[0];
    }
    return truncated?0:actualend;

truncate:
    end[-1] = 0;
    return 0;
}

