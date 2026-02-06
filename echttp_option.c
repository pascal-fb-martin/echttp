/* echttp - Embedded HTTP server.
 *
 * Copyright 2020, Pascal Martin
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
 * This module provides functions meant to make decoding command line options
 * easier.
 *
 * const char *echttp_option_match (const char *reference,
 *                                  const char *input, const char **value);
 *
 *    Return 0 if the argument does not match the reference string, a pointer
 *    to the value otherwise.
 *
 *    The value parameter is optional: if not used, just set it to 0.
 *
 *    This function supports the syntax option '=' value, if the reference
 *    string ends with a '='. In this case, and if there is a match,
 *    value points to the string after the '='. Otherwise value is not
 *    touched, so that the caller can initialize it with a default value.
 *
 * int echttp_option_csv (const char *reference,
 *                        const char *input,
 *                        char *values[], int max);
 *
 *    Return 0 if the argument does not match the reference string, or
 *    the count of returned values otherwise. The "values" parameter is
 *    an array of size "max". The argument must follow a comma separated
 *    format. For example:
 *
 *        -option=value1,value2,value3
 *
 *    The strings referenced in the array have been allocated as a single
 *    block, corresponding to the element 0 of the array. The memory can
 *    (should) be deallocated by doing a single free() call:
 *
 *        free (values[0]);
 *
 *    If echttp_option_csv() returns 0, the existing content of array `values`
 *    was not modified.
 *
 * int echttp_option_present (const char *reference, const char *input);
 *
 *    Return 1 if the argument exactly match the reference string.
 *
 *    This is typically used for boolean options.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp.h"


const char *echttp_option_match (const char *reference,
                                 const char *input, const char **value) {

    size_t length = strlen(reference);

    // Accept both the -name=value and --name=value variants
    // if the expected option is -name=value.
    //
    if ((input[0] == '-' && input[1] == '-') &&
        (reference[0] == '-' && reference[1] != '-')) input += 1;

    if (strncmp (reference, input, length)) return 0;

    if (input[length] != 0) {
        if (input[length-1] != '=') return 0;
        if (value) *value = input + length;
    }
    return input + length;
}

int echttp_option_csv (const char *reference,
                       const char *input,
                       char *values[], int max) {

    if (max < 1) return 0;
    const char *value = echttp_option_match (reference, input, 0);
    if (!value) return 0;
    if (*value == 0) return 0;

    char *copy = strdup (value);
    char *cursor = copy;
    int count = 1;
    values[0] = copy;
    while (*(++cursor)) {
        if (*cursor != ',') continue;
        *(cursor++) = 0;
        if (*cursor == 0) return count; // Protect against terminal ','.
        if (count >= max) return count; // Protect against overflow.
        values[count++] = cursor;
    }
    return count;
}

int echttp_option_present (const char *reference, const char *input) {

    // Accept both the -name and --name variants
    // if the expected option is -name.
    //
    if ((input[0] == '-' && input[1] == '-') &&
        (reference[0] == '-' && reference[1] != '-')) input += 1;

    return strcmp (reference, input) == 0;
}

