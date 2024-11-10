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
 * A module to handle the HTTP encoding rules.
 *
 * char *echttp_encoding_escape (const char *s, char *d, int size);
 *
 *    This function encodes the s string and stores the result to d.
 *    The result string cannot be longer than the specified size.
 *    The d parameter is returned (to allow using the function in a parameter
 *    list).
 *
 * char *echttp_encoding_unescape (char *data);
 *
 *    This function decodes the HTTP escape sequence and restore the original
 *    data. The change is made in the original string. The data parameter is
 *    returned (to allow using this function in a parameter list).
 *
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "echttp_encoding.h"

static int echttp_encoding_hextoi (char a) {
    if (isdigit(a)) return a - '0';
    return tolower(a) - 'a' + 10;
}

static char echttp_encoding_itohex (unsigned int a) {
    if (a > 15) return 'F';
    if (a < 0)  return '0';
    if (a < 10) return '0' + a;
    return 'A' + a - 10;
}

static char echttp_encoding_table[128]; // Encoding for punctuation.

static void echttp_encoding_init (void) {
    echttp_encoding_table['%'] = 1; // Used as the "initialized" flag.
    echttp_encoding_table[','] = 1;
    echttp_encoding_table['/'] = 1;
    echttp_encoding_table[':'] = 1;
    echttp_encoding_table[';'] = 1;
    echttp_encoding_table['<'] = 1;
    echttp_encoding_table['='] = 1;
    echttp_encoding_table['>'] = 1;
    echttp_encoding_table['?'] = 1;
    echttp_encoding_table['@'] = 1;
    echttp_encoding_table['['] = 1;
    echttp_encoding_table['\\'] = 1;
    echttp_encoding_table[']'] = 1;
    echttp_encoding_table['^'] = 1;
    echttp_encoding_table['`'] = 1;
}

char *echttp_encoding_unescape (char *data) {

    char *f = data;
    char *t = data;
    while (*f) {
        if (*f == '%') {
            if ((!isxdigit(f[1])) || (!isxdigit(f[2]))) return 0;
            *t = 16 * echttp_encoding_hextoi(f[1])
                    + echttp_encoding_hextoi(f[2]);
            f += 2;
        } else if (t != f) {
            *t = *f;
        }
        t += 1;
        f += 1;
    }
    *t = 0; // Force a terminator.
    return data;
}

char *echttp_encoding_escape (const char *s, char *d, int size) {
    int i = 0;
    if (!echttp_encoding_table['%']) echttp_encoding_init();

    size -= 1; // Reserve space for the null terminator.
    while (i < size) {
        int c = *(s++) & 0xff;
        if (c <= 41 || c >= 123 || echttp_encoding_table[c]) {
            if (c <= 0) break;
            if (i >= size - 3) break;
            d[i++] = '%';
            d[i++] = echttp_encoding_itohex (c >> 4);
            d[i++] = echttp_encoding_itohex (c & 0x0f);
        } else {                           // Compatible character.
            d[i++] = (char)c;
        }
    }
    d[i] = 0;
    return d;
}

