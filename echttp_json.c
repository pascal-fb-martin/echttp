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
 *
 * A minimal JSON decoder design for simple interface and minimal use
 * of resources.
 *
 * This JSON decoder was inpired by JSMN (https://github.com/zserge/jsmn),
 * but is a totally independent implementation, using recursive descent
 * instead of a state machine.
 *
 * int echttp_json_estimate (const char *json);
 *
 * const char *echttp_json_parse (char *json, ParserToken *token, int *count);
 *
 *    Decode a JSON string and return a list of tokens. The decoding breaks
 *    the input string.
 *
 * int echttp_json_search (const ParserToken *token, int max, const char *path);
 *
 *    Retrieve a JSON item (after decoding) and return its index. return -1
 *    if the item was not found.
 *
 * const char *echttp_json_enumerate (const ParserToken *parent, int *index);
 *
 *    Populate the list of children items to an array or object's parent.
 *    The index array must be large enough for the expected number of
 *    children (as indicated by parent->length). The indexes provided are
 *    relative to the parent's token.
 *    Return null on success, an error text on failure.
 *
 * ParserContext echttp_json_start
 *                 (ParserToken *token, int max, char *pool, int size);
 * void echttp_json_add_null
 *          (ParserContext context, int parent, const char *key);
 * void echttp_json_add_bool
 *          (ParserContext context, int parent, const char *key, int value);
 * void echttp_json_add_integer
 *          (ParserContext context, int parent, const char *key, long value);
 * void echttp_json_add_real
 *          (ParserContext context, int parent, const char *key, double value);
 * void echttp_json_add_string
 *          (ParserContext context, int parent, const char *key, const char *value);
 * int echttp_json_add_object
 *          (ParserContext context, int parent, const char *key);
 * int echttp_json_add_array
 *          (ParserContext context, int parent, const char *key);
 * int echttp_json_end (ParserContext context);
 *
 *    The functions above can be used to generate a JSON token list without
 *    parsing JSON text. This is typically used when building a JSON response.
 *
 * const char *echttp_json_format (ParserToken *token, int count,
 *                                 char *json, int size, int options);
 *
 *    Format a JSON string given an array of tokens. Use the option
 *    PRINT_OPTION_PRETTY to make the output readable.
 *
 * const char *echttp_json_export (ParserContext context, char *buffer, int size);
 *
 *    This function combines echttp_json_end() and echttp_json_format()
 *    without pretty formatting, which is what most web servers will tend
 *    to use when generating a response..
 */

#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "echttp.h"
#include "echttp_json.h"

#define JSON_MAX_DEPTH 64

/* This data structure holds the current state of the parser. It is meant
 * to make it easy to pass the current content from one level to the next.
 * (See echttp_json.h for the ParserContext opaque type.)
 */
struct ParserContext_s {
    int cursor;
    int count;

    int line_count;
    int line_start;
    char *error;

    char *source;
    ParserToken *token;
    int max;

    int size;
    int depth;
    int ending[32];
    int countdown[32];
    int options;
};

static int  echttp_json_debug = 0;
static char echttp_json_error_text[160];

static const char *echttp_json_object (ParserContext context);
static const char *echttp_json_array (ParserContext context);

#define JSONTRACE(x) if (echttp_json_debug) printf ("%s at line %d column %d: %s\n", x, context->line_count, context->cursor - context->line_start + 1, context->source + context->cursor);

static char skip_spaces (ParserContext context) {

    char *json = context->source + context->cursor;

    for (;;) {
        while (isspace(*json)) {
            if (*json == '\n') {
                context->line_count += 1;
                context->line_start = context->cursor + 1;
            }
            context->cursor += 1;
            json += 1;
        }
        if (json[0] != '/' || json[1] != '/') break;

        json = strchrnul (json+2, '\n'); // Next line.
        context->cursor = (int)(json - context->source);
    }

    JSONTRACE ("next word");
    return *json;
}

static char next_word (ParserContext context) {
    context->cursor += 1;
    return skip_spaces (context);
}

static const char *add_token (ParserContext context) {
    context->count += 1;
    if (context->count > context->max) return "JSON structure is too long";
    return 0;
}

static const char *echttp_json_literal (ParserContext context) {

   char *json = context->source + context->cursor;
   ParserToken *token = context->token;

   JSONTRACE ("literal");
   if (strncmp (json, "true", 4) == 0) {
       token[context->count].type = PARSER_BOOL;
       token[context->count].value.bool = 1;
       context->cursor += 3;
   } else if (strncmp (json, "false", 5) == 0) {
       token[context->count].type = PARSER_BOOL;
       token[context->count].value.bool = 0;
       context->cursor += 4;
   } else if (strncmp (json, "null", 4) == 0 && (! isalnum(json[4]))) {
       token[context->count].type = PARSER_NULL;
       context->cursor += 3;
   } else {
       return "invalid literal";
   }
   if (isalnum(context->source[context->cursor+1])) return "invalid literal";
   token[context->count].length = 0;
   return 0;
}

static const char *echttp_json_number (ParserContext context) {

    static char isvalidnumber[128] = {0};

    char *json = context->source + context->cursor;

    if (!isvalidnumber['0']) {
        int i;
        isvalidnumber['-'] = 1;
        for (i = '0'; i <= '9'; ++i) isvalidnumber[i] = 1;
        isvalidnumber['.'] = 2; // Presence indicates a real number.
        isvalidnumber['e'] = 2;
        isvalidnumber['E'] = 2;
    }
    JSONTRACE ("number");
    while (*json > 0 && isvalidnumber[*json] == 1) json += 1;
    if (*json > 0 && isvalidnumber[*json] == 2) {
        context->token[context->count].type = PARSER_REAL;
        context->token[context->count].value.real =
            strtof(context->source + context->cursor, &json);
    } else {
        context->token[context->count].type = PARSER_INTEGER;
        context->token[context->count].value.integer =
            strtol(context->source + context->cursor, &json, 0);
    }
    context->token[context->count].length = 0;
    context->cursor = (int) (json - context->source) - 1;
    return 0;
}

static char hex2bin(char x) {
    if (x >= '0' && x <= '9') return x - '0';
    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
    return -1;
}

static const char *echttp_json_string (ParserContext context) {
    char *from = context->source + context->cursor + 1;
    char *to = from;
    int l, h, ucode;

    context->token[context->count].type = PARSER_STRING;
    context->token[context->count].length = 0;
    context->token[context->count].value.string = to;

    JSONTRACE ("string");
    for (;;) {
        switch (*from) {
            case '\"':
                *to = 0;
                context->cursor = (int)(from - context->source);
                return 0;
            case '\\':
                switch (*++from) {
                    case '\"':
                    case '\\':
                    case '/': *to++ = *from; break;
                    case 'b': *to++ = 8; break;
                    case 'f': *to++ = 12; break;
                    case 'n': *to++ = 10; break;
                    case 'r': *to++ = 13; break;
                    case 't': *to++ = 9; break;
                    case 'u':
                       h = hex2bin(from[1]);
                       l = hex2bin(from[2]);
                       if (h < 0 || l < 0) return "invalid unicode";
                       ucode = (16 * h + l) << 8;
                       h = hex2bin(from[3]);
                       l = hex2bin(from[4]);
                       if (h < 0 || l < 0) return "invalid unicode";
                       ucode += 16 * h + l;
                       if (ucode < 0xd800 || ucode >= 0xe000) {
                           // Convert UTF-16 to UTF-8:
                           if (ucode < 0x80) {
                               *to++ = (char) (ucode & 0x7f);
                               if (echttp_json_debug) printf ("UTF-16 %04x to UTF-8 %02x\n", ucode, (int)(to[-1]) & 0xff);
                           } else if (ucode < 0x800) {
                               *to++ = 0xc0 + ((ucode >> 6) & 0x1f);
                               *to++ = 0x80 + (ucode & 0x3f);
                               if (echttp_json_debug) printf ("UTF-16 %04x to UTF-8 %02x %02x\n", ucode, (int)(to[-2]) & 0xff, (int)(to[-1]) & 0xff);
                           } else {
                               *to++ = 0xe0 + ((ucode >> 12) & 0x1f);
                               *to++ = 0x80 + ((ucode >> 6) & 0x3f);
                               *to++ = 0x80 + (ucode & 0x3f);
                               if (echttp_json_debug) printf ("UTF-16 %04x to UTF-8 %02x %02x %02x\n", ucode, (int)(to[-3]) & 0xff, (int)(to[-2]) & 0xff, (int)(to[-1]) & 0xff);
                           }
                           from += 4;
                       } else if (ucode >= 0xdc00) {
                           return "missing 1st half of surrogate pair";
                       } else {
                           if (from[5] != '\\' || from[6] != 'u')
                               return "missing 2nd half of surrogate pair";
                           // UTF-32 coded as a surrogate pair.
                           int pair1 = ucode - 0xd800;
                           h = hex2bin(from[7]);
                           l = hex2bin(from[8]);
                           if (h < 0 || l < 0) return "invalid unicode";
                           int pair2 = (16 * h + l) << 8;
                           h = hex2bin(from[9]);
                           l = hex2bin(from[10]);
                           if (h < 0 || l < 0) return "invalid unicode";
                           pair2 = pair2 + 16 * h + l - 0xdc00;
                           if (pair2 < 0 || pair2 > 0x3ff)
                               return "invalid UTF-16 surrogate pair";
                           ucode = 0x10000 + (pair1 << 10) + pair2;
                           // Convert UTF-32 to UTF-8:
                           *to++ = 0xf0 + ((ucode >> 18) & 7);
                           *to++ = 0x80 + ((ucode >> 12) & 0x3f);
                           *to++ = 0x80 + ((ucode >> 6) & 0x3f);
                           *to++ = 0x80 + (ucode & 0x3f);
                           if (echttp_json_debug) printf ("UTF-32 %06x to UTF-8 %02x %02x %02x %02x\n", ucode, (int)(to[-4]) & 0xff, (int)(to[-3]) & 0xff, (int)(to[-2]) & 0xff, (int)(to[-1]) & 0xff);
                           from += 10;
                       }
                       break;
                }
                from += 1;
                break;
            case 0:
                return "unterminated string";
            default:
                if (to != from) *to = *from;
                to++; from++;
        }
    }

    return "unterminated string";
}

static const char *echttp_json_value (ParserContext context) {

    const char *error = 0;
    char c = next_word(context);

    JSONTRACE ("value");

    switch (c) {

        case '[': error = echttp_json_array (context); break;

        case '{': error = echttp_json_object (context); break;

        case '\"': error = echttp_json_string (context); break;

        case 'f': case 't': case 'n':
            error = echttp_json_literal (context);
            break;

        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
        case '-':
            error = echttp_json_number (context);
            break;

        default:
            return "invalid character, expected a value";
    }
    return error;
}

static const char *echttp_json_array (ParserContext context) {

    int i = context->count;
    ParserToken *token = context->token;
    char *json = context->source;

    token[i].type = PARSER_ARRAY;
    token[i].length = 0;

    JSONTRACE ("array");
    for (;;) {
        const char *error = add_token(context);
        if (error) return error;

        token[context->count].key = 0;
        error = echttp_json_value (context);
        if (error) {
            if (json[context->cursor] == ']') {
                context->count -= 1; //Backtrack the added token.
                return 0; // Not an error.
            }
            return error;
        }

        token[i].length += 1;

        switch (next_word(context)) {
        case ']': return 0;
        case '}': return "array terminated as an object";
        case ',': break;
        default: return "invalid array value separator";
        }
    }
    return "array processing error";
}

static const char *echttp_json_object (ParserContext context) {

    int i = context->count;
    ParserToken *token = context->token;
    char *json = context->source;

    token[i].type = PARSER_OBJECT;
    token[i].length = 0;

    JSONTRACE ("object");
    for (;;) {
        switch (next_word(context)) {
        case '}': return 0;
        case '\"':
            if (token[i].length) return "missing separator";
            break;
        case ',':
            if (!token[i].length) return "missing value";
            if (next_word(context) == '\"') break;
        default: return "invalid character, expected a string (key)";
        }

        const char *error = add_token(context);
        if (error) return error;

        error = echttp_json_string (context);
        if (error) return error;
        token[context->count].key = token[context->count].value.string;

        if (next_word(context) != ':')
            return "invalid separator, expected a ':' after a key";

        error = echttp_json_value (context);
        if (error) return error;

        token[i].length += 1;
    }
    return "object processing error";
}

int echttp_json_estimate (const char *json) {
    // This method of counting does not escape the literal strings content
    // and does not account for a ',' after an object or array: it might
    // overestimates the number of tokens needed. This is OK because
    // we are looking for enough space, not for the smallest space.
    int count = 0;
    for (;;) {
        switch (*(json++)) {
            case ']':
            case '}':
                count += 2; // the item before, plus the object/array.
                break;
            case ',':
                count += 1;
                break;
            case 0:
                return count;
        }
    }
}

void echttp_json_enable_debug (void) {
    echttp_json_debug = 1;
}

const char *echttp_json_parse (char *json, ParserToken *token, int *count) {

   const char *error;
   struct ParserContext_s context;

   if (!count || !*count) return "invalid count parameter";

   context.line_count = 1;
   context.line_start = 0;
   context.cursor = context.count = 0;

   context.source = json;
   context.token = token;
   context.max = *count;

   echttp_json_error_text[0] = 0;
   *count = 0;

   token[0].key = 0;

   switch (skip_spaces (&context)) {
       case 0:   error = "no data"; break;
       case '{': error = echttp_json_object (&context); break;
       case '[': error = echttp_json_array(&context); break;
       default:  error = echttp_json_value (&context);
   }
   *count = context.count + 1;

   if (! error) {
       if (next_word(&context) != 0) error = "data left at the end of input";
   }

   if (error) {
       snprintf (echttp_json_error_text, sizeof(echttp_json_error_text),
                 "%s at line %d, column %d\n",
                 error, context.line_count, context.cursor-context.line_start);
       return echttp_json_error_text;
   }
   return 0;
}


static void echttp_json_gen_append (ParserContext context, const char *text) {

    int length = strlen(text);

    if (context->cursor + length < context->size) {
        strncpy (context->source+context->cursor,
                 text, context->size-context->cursor);
        context->source[context->size-1] = 0;
        context->cursor += length;
    }
}

static void echttp_json_gen_indent (ParserContext context) {

    if (context->options & PRINT_OPTION_PRETTY) {
        static char indent[81];
        if (indent[0] == 0) memset (indent, ' ',sizeof(indent)-1);
        const char *sp = indent + sizeof(indent) -1 - (4 * context->depth);
        echttp_json_gen_append (context, sp);
    }
}

static void echttp_json_gen_key (ParserContext context, const char *key) {
    echttp_json_gen_indent (context);
    if (key) {
        const char *sep = (context->options & PRINT_OPTION_PRETTY)?"\" : ":"\":";
        echttp_json_gen_append (context, "\"");
        echttp_json_gen_append (context, key);
        echttp_json_gen_append (context, sep);
    }
}

static void echttp_json_gen_eol (ParserContext context, int comma) {
    if (context->options & PRINT_OPTION_PRETTY) {
        if (comma)
            echttp_json_gen_append (context, ",\n");
        else
            echttp_json_gen_append (context, "\n");
    } else if (comma) {
        echttp_json_gen_append (context, ",");
    }
}

static void echttp_json_gen_bool (ParserContext context, int i) {
    echttp_json_gen_append (context,
                            context->token[i].value.bool?"true":"false");
}

static void echttp_json_gen_integer (ParserContext context, int i) {
    char buffer[32];
    snprintf (buffer, sizeof(buffer), "%ld", context->token[i].value.integer);
    echttp_json_gen_append (context, buffer);
}

static void echttp_json_gen_real (ParserContext context, int i) {
    char buffer[64];
    snprintf (buffer, sizeof(buffer), "%e", context->token[i].value.real);
    echttp_json_gen_append (context, buffer);
}

static char *echttp_json_gen_utf16 (ParserContext context, int value, char *to) {

    static const char tohex [16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    *to++ = '\\';
    *to++ = 'u';
    *to++ = tohex[(value >> 12) & 0x0f];
    *to++ = tohex[(value >> 8) & 0x0f];
    *to++ = tohex[(value >> 4) & 0x0f];
    *to++ = tohex[value & 0x0f];
    return to;
}

static void echttp_json_gen_string (ParserContext context, int i) {

    static char escapelist [128];

    const char *value = context->token[i].value.string;
    if (!value || value[0] == 0) {
        echttp_json_gen_append (context, "\"\"");
        return;
    }

    char *buffer = (char *)malloc (6*strlen(value) + 3); // Worst case.
    char *to = buffer;

    if (escapelist['n'] == 0) {
        // Do not escape '/': used for URLs and not necessary anyway.
        escapelist['\"'] = '\"';
        escapelist['\\'] = '\\';
        escapelist[8] = 'b';
        escapelist[12] = 'f';
        escapelist[10] = 'n';
        escapelist[13] = 'r';
        escapelist[9] = 't';
    }

    *to++ = '"';

    while (*value) {
        if (*value > 0) {
            char escape = escapelist[*value];
            if (escape) {
                *to++ = '\\';
                *to++ = escape;
                value++;
            } else {
                *to++ = *value++;
            }
        } else {
            unsigned int lead = (unsigned int)(*value) & 0xff;
            if (lead >= 0xf1) {
                value += 1; // Invalid UTF8 character, ignore.
                continue;
            }
            int ucode;
            int incr = 1;
            if (lead < 0xe0) {
                // UTF-16 (2 bytes).
                ucode = (((int)(value[0]) & 0x1f) << 6) +
                        ((int)(value[1]) & 0x3f);
                to = echttp_json_gen_utf16 (context, ucode, to);
                if (echttp_json_debug) printf ("UTF-8 %02x %02x to UTF-16 %04x\n", (int)(value[0]) & 0xff, (int)(value[1]) & 0xff, ucode);
                if (value[1] < 0) incr += 1;
            } else if (lead < 0xf0) {
                // UTF-16 (3 bytes).
                ucode = (((int)(value[0]) & 0x0f) << 12) +
                        (((int)(value[1]) & 0x3f) << 6) +
                        ((int)(value[2]) & 0x3f);
                to = echttp_json_gen_utf16 (context, ucode, to);
                if (echttp_json_debug) printf ("UTF-8 %02x %02x %02x to UTF-16 %04x\n", (int)(value[0]) & 0xff, (int)(value[1]) & 0xff, (int)(value[2]) & 0xff, ucode);
                if (value[1] < 0) {
                    incr += 1;
                    if (value[2] < 0) incr += 1;
                }
            } else {
                // UTF-16 surrogate pair (4 bytes).
                ucode = (((int)(value[0]) & 0x7) << 18) +
                        (((int)(value[1]) & 0x3f) << 12) +
                        (((int)(value[2]) & 0x3f) << 6) +
                        ((int)(value[3]) & 0x3f);
                int pair1 = 0xd800 + ((ucode - 0x10000) >> 10);
                int pair2 = 0xdc00 + ((ucode - 0x10000) & 0x3ff);
                if (echttp_json_debug) printf ("UTF-8 %02x %02x %02x %02x to UTF-32 %06x, UTF-16 %04x %04x\n", (int)(value[0]) & 0xff, (int)(value[1]) & 0xff, (int)(value[2]) & 0xff, (int)(value[3]) & 0xff, ucode, pair1, pair2);
                to = echttp_json_gen_utf16 (context, pair1, to);
                to = echttp_json_gen_utf16 (context, pair2, to);
                if (value[1] < 0) {
                    incr += 1;
                    if (value[2] < 0) {
                        incr += 1;
                        if (value[3] < 0) incr += 1;
                    }
                }
            }
            value += incr;
        }
    }
    *to++ = '"';
    *to = 0;
    echttp_json_gen_append (context, buffer);
    free(buffer);
}

const char *echttp_json_format (ParserToken *token, int count,
                                char *json, int size, int options) {

    struct ParserContext_s context;

    int i;

    context.source = json;
    context.size = size;
    context.token = token;
    context.max = context.count = count;
    context.options = options;

    context.depth = 0;
    context.cursor = 0;
    context.countdown[0] = 0;

    for (i = 0; i < count; ++i) {

        int comma = context.countdown[context.depth] > 1;

        echttp_json_gen_key (&context, token[i].key);

        switch (token[i].type) {
            case PARSER_NULL:
                echttp_json_gen_append (&context, "null"); break;
            case PARSER_BOOL:
                echttp_json_gen_bool (&context, i); break;
            case PARSER_INTEGER:
                echttp_json_gen_integer (&context, i); break;
            case PARSER_REAL:
                echttp_json_gen_real (&context, i); break;
            case PARSER_STRING:
                echttp_json_gen_string (&context, i); break;
            case PARSER_ARRAY:
                echttp_json_gen_append (&context, "[");
                context.depth += 1;
                context.ending[context.depth] = ']';
                context.countdown[context.depth] = token[i].length + 1;
                comma = 0;
                break;
            case PARSER_OBJECT:
                echttp_json_gen_append (&context, "{");
                context.depth += 1;
                context.ending[context.depth] = '}';
                context.countdown[context.depth] = token[i].length + 1;
                comma = 0;
                break;
            default:
                fprintf (stderr, "Invalid token type %d at index %d\n",
                         token[i].type, i);
                return "invalid token type";
        }

        echttp_json_gen_eol (&context, comma);

        while (context.depth > 0) {
            char buffer[2];
            context.countdown[context.depth] -= 1;
            if (context.countdown[context.depth] > 0) break;
            buffer[0] = context.ending[context.depth]; buffer[1] = 0;
            context.depth -= 1;
            echttp_json_gen_indent (&context);
            echttp_json_gen_append (&context, buffer);
            echttp_json_gen_eol (&context, context.countdown[context.depth]>1);
        }
    }
    if (context.depth > 0) return "unfinished structure";
    if (context.cursor >= context.size) return "not enough space";
    return 0;
}


static const char *next_separator (const char *p) {
    while (*p) {
        if (*p == '.') return p;
        if (*p == '[') return p;
        p++;
    }
    return p;
}

static int search_array_element (const ParserToken *token,
                                 const char *path, int index) {
    int depth = 0;
    int stack[JSON_MAX_DEPTH];
    int i;
    int count = index;

    for (i = 0; i <= count; i++) {

        // Skip deeper elements.

        while (depth > 0) {
            if (--stack[depth] > 0) break;
            depth -= 1;
        }
        if (depth == 0) {
            if (index == 0) {
                int d = echttp_json_search (token+i, path);
                if (d < 0) return -1;
                return i + d;
            }
            index--;
        }
        if (token[i].length > 0) {
            count += token[i].length;
            if (depth >= JSON_MAX_DEPTH-1) return -1;
            stack[++depth] = token[i].length + 1;
        }
    }
    return -1;
}

static int search_object_element (const ParserToken *parent, const char *path) {

    const char *p = next_separator(path);
    int length = (int) (p - path);
    int depth = 0;
    int stack[JSON_MAX_DEPTH];
    int i;
    int count = parent->length;
    int instance = -1;

    if (*path == 0) return 0;

    for (i = 1; i <= count; i++) {

        // Skip deeper elements.

        while (depth > 0) {
            if (--stack[depth] > 0) break;
            depth -= 1;
        }

        if (depth == 0) {
            int match = 0;
            if (parent[i].key && length) {
                match = (strncmp (path, parent[i].key, length) == 0);
            } else if (parent[i].key == 0 && length == 0) {
                match = 1;
            }
            if (match) {
                if (instance < 0 &&
                        *p == '[' && parent[i].type == PARSER_OBJECT) {
                    // This can happen in XML (tag name repeated).
                    char *end;
                    instance = strtol(p+1, &end, 0);
                    if (*end != ']' || instance < 0) return -1;
                    p = end + 1;
                }
                if (instance > 0) {
                    instance -= 1;
                } else {
                    int d = echttp_json_search (parent+i, p);
                    if (d < 0) return -1;
                    return i + d;
                }
            }
        }
        if (parent[i].length > 0) {
            count += parent[i].length;
            if (depth >= JSON_MAX_DEPTH-1) return -1;
            stack[++depth] = parent[i].length + 1;
        }
    }
    return -1;
}

int echttp_json_search (const ParserToken *parent, const char *path) {

    if (*path == 0) return 0; // End of search: any type is fine.

    if (*path == '.' && parent->type == PARSER_OBJECT) {
        return search_object_element (parent, path+1);
    }

    if (*path == '[' && parent->type == PARSER_ARRAY) {
        char *end;
        int d;
        int i = strtol(path+1, &end, 0);
        if (*end != ']' || i < 0 || i >= parent->length) return -1;
        d = search_array_element (parent+1, end+1, i);
        if (d < 0) return -1;
        return d + 1;
    }
    return -1; // Not the type of token we expected.
}

const char *echttp_json_enumerate (const ParserToken *parent, int *index) {

    int depth = 0;
    int stack[JSON_MAX_DEPTH];
    int i;
    int child = 0;
    int count = parent->length;

    if (count == 0) {
        if (parent->type != PARSER_ARRAY && parent->type != PARSER_OBJECT)
            return "invalid type";
        return "no data";
    }

    for (i = 1; i <= count; i++) {

        // Skip deeper elements.

        while (depth > 0) {
            if (--stack[depth] > 0) break;
            depth -= 1;
        }
        if (depth == 0) {
            if (child >= parent->length) return "too many items found";
            index[child++] = i;
        }

        if (parent[i].length > 0) {
            count += parent[i].length;
            if (depth >= JSON_MAX_DEPTH-1) return "data structure too deep";
            stack[++depth] = parent[i].length + 1;
        }
    }
    if (child != parent->length) return "too few items found";

    return 0;
}

ParserContext echttp_json_start
                (ParserToken *token, int max, char *pool, int size) {
    ParserContext context = (ParserContext) malloc(sizeof(*context));

    context->token = token;
    context->max = max;
    context->count = 0;

    context->source = pool;
    context->size = size;
    context->cursor = 0;
    return context;
}

static char *echttp_json_add_pool (ParserContext context, const char *text) {
    if (!text) return 0;
    if (context->cursor < context->size) {
        char *p = context->source + context->cursor;
        strncpy (p, text, context->size-context->cursor);
        context->source[context->size-1] = 0;
        context->cursor += strlen(context->source+context->cursor) + 1;
        return p;
    }
    return 0;
}

static ParserToken *echttp_json_add_token
                      (ParserContext context, int parent, const char *key) {

    ParserToken *token = context->token + context->count;

    if (context->count >= context->max) return 0;
    token->length = 0;
    if (context->count == 0) {
        token->key = 0;
        context->count = 1; // Root element.
        return token;
    } else if (parent >= 0 && parent < context->count) {
        if (context->token[parent].type == PARSER_OBJECT)
            token->key = echttp_json_add_pool (context, key);
        else if (context->token[parent].type == PARSER_ARRAY)
            token->key = 0;
        else
            return 0;
        context->token[parent].length += 1;
        context->count += 1;
        return token;
    }
    return 0;
}

void echttp_json_add_null
         (ParserContext context, int parent, const char *key) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_NULL;
    }
}

void echttp_json_add_bool
         (ParserContext context, int parent, const char *key, int value) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_BOOL;
        token->value.bool = (value != 0);
    }
}

void echttp_json_add_integer
         (ParserContext context, int parent, const char *key, long value) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_INTEGER;
        token->value.integer = value;
    }
}

void echttp_json_add_real
         (ParserContext context, int parent, const char *key, double value) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_REAL;
        token->value.real = value;
    }
}

void echttp_json_add_string
         (ParserContext context, int parent, const char *key, const char *value) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_STRING;
        token->value.string = echttp_json_add_pool (context, value);
    }
}

int echttp_json_add_object
         (ParserContext context, int parent, const char *key) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_OBJECT;
    }
    return (int)(token - context->token);
}

int echttp_json_add_array
         (ParserContext context, int parent, const char *key) {
    ParserToken *token = echttp_json_add_token (context, parent, key);
    if (token) {
        token->type = PARSER_ARRAY;
    }
    return (int)(token - context->token);
}

int echttp_json_end (ParserContext context) {
    int count = context->count;
    free (context);
    return count;
}

const char *echttp_json_export (ParserContext context, char *buffer, int size) {
    const char *error =
        echttp_json_format (context->token, context->count, buffer, size, 0);
    if (!error && context->count >= context->max) error = "token array is full";
    echttp_json_end (context);
    return error;
}

