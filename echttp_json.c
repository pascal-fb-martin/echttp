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
 * const char *echttp_json_parse (char *json, JsonToken *token, int *count);
 *
 *    Decode a JSON string and return a list of tokens. The decoding breaks
 *    the input string.
 *
 * int echttp_json_search (const JsonToken *token, int max, const char *path);
 *
 *    Retrieve a JSON item (after decoding) and return its index. return -1
 *    if the item was not found.
 *
 * const char *echttp_json_enumerate (const JsonToken *parent, int *index);
 *
 *    Populate the list of children items to an array or object's parent.
 *    The index array must be large enough for the expected number of
 *    children (as indicated by parent->length). The indexes provided are
 *    relative to the parent's token.
 *    Return null on success, an error text on failure.
 *
 * const char *echttp_json_generate (JsonToken *token, int count,
 *                                   char *json, int size, int options);
 *
 *    Generate a JSON string give an array of tokens.
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
 * (See echttp_json.h for the JsonContext opaque type.)
 */
struct JsonContext_s {
    int cursor;
    int count;

    int line_count;
    int line_start;
    char *error;

    char *json;
    JsonToken *token;
    int max;

    int size;
    int depth;
    int ending[32];
    int countdown[32];
    int options;
};

static int  echttp_json_debug = 0;
static char echttp_json_error_text[160];

static const char *echttp_json_object (JsonContext context);
static const char *echttp_json_array (JsonContext context);

#define JSONTRACE(x) if (echttp_json_debug) printf ("%s at line %d column %d: %s\n", x, context->line_count, context->cursor - context->line_start + 1, context->json + context->cursor);

static char skip_spaces (JsonContext context) {

    char *json = context->json + context->cursor;

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
        context->cursor = (int)(json - context->json);
    }

    JSONTRACE ("next word");
    return *json;
}

static char next_word (JsonContext context) {
    context->cursor += 1;
    return skip_spaces (context);
}

static const char *add_token (JsonContext context) {
    context->count += 1;
    if (context->count > context->max) return "JSON structure is too long";
    return 0;
}

static const char *echttp_json_literal (JsonContext context) {

   char *json = context->json + context->cursor;
   JsonToken *token = context->token;

   JSONTRACE ("literal");
   if (strncmp (json, "true", 4) == 0) {
       token[context->count].type = JSON_BOOL;
       token[context->count].value.bool = 1;
       context->cursor += 3;
   } else if (strncmp (json, "false", 5) == 0) {
       token[context->count].type = JSON_BOOL;
       token[context->count].value.bool = 0;
       context->cursor += 4;
   } else if (strncmp (json, "null", 4) == 0 && (! isalnum(json[4]))) {
       token[context->count].type = JSON_NULL;
       context->cursor += 3;
   } else {
       return "invalid literal";
   }
   if (isalnum(context->json[context->cursor+1])) return "invalid literal";
   token[context->count].length = 0;
   return 0;
}

static const char *echttp_json_number (JsonContext context) {

    static char isvalidnumber[128] = {0};

    char *json = context->json + context->cursor;

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
        context->token[context->count].type = JSON_REAL;
        context->token[context->count].value.real =
            strtof(context->json + context->cursor, &json);
    } else {
        context->token[context->count].type = JSON_INTEGER;
        context->token[context->count].value.integer =
            strtol(context->json + context->cursor, &json, 0);
    }
    context->token[context->count].length = 0;
    context->cursor = (int) (json - context->json) - 1;
    return 0;
}

static char hex2bin(char x) {
    if (x >= '0' && x <= '9') return x - '0';
    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
    return -1;
}

static const char *echttp_json_string (JsonContext context) {
    char *from = context->json + context->cursor + 1;
    char *to = from;
    int l, h, ucode;

    context->token[context->count].type = JSON_STRING;
    context->token[context->count].length = 0;
    context->token[context->count].value.string = to;

    JSONTRACE ("string");
    for (;;) {
        switch (*from) {
            case '\"':
                *to = 0;
                context->cursor = (int)(from - context->json);
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
                       if (from[5] != '\\' || from[6] != 'u') {
                           // Convert UTF-16 to UTF-8:
                           if (ucode < 0x80) {
                               *to++ = (char) (ucode & 0x7f);
                           } else if (ucode < 0x800) {
                               *to++ = 0xc0 + ((ucode >> 6) & 0x1f);
                               *to++ = 0x80 + (ucode & 0x3f);
                           } else if (ucode >= 0xD800) {
                               return "reserved UTF-16 character";
                           } else {
                               *to++ = 0xc0 + ((ucode >> 12) & 0x1f);
                               *to++ = 0x80 + ((ucode > 6) & 0x3f);
                               *to++ = 0x80 + (ucode & 0x3f);
                           }
                           from += 4;
                       } else {
                           // UTF-32 coded as a surrogate pair.
                           ucode -= 0xd800;
                           if (ucode < 0 || ucode > 0x3ff)
                               return "invalid UTF-16 surrogate pair";
                           int pair2;
                           h = hex2bin(from[7]);
                           l = hex2bin(from[8]);
                           if (h < 0 || l < 0) return "invalid unicode";
                           pair2 = (16 * h + l) << 8;
                           h = hex2bin(from[9]);
                           l = hex2bin(from[10]);
                           if (h < 0 || l < 0) return "invalid unicode";
                           pair2 = pair2 + 16 * h + l - 0xdc00;
                           if (pair2 < 0 || pair2 > 0x3ff)
                               return "invalid UTF-16 surrogate pair";
                           ucode = 0x10000 + (ucode << 10) + pair2;
                           // Convert UTF-32 to UTF-8:
                           *to++ = 0xf0 + ((ucode >> 18) & 7);
                           *to++ = 0x80 + ((ucode >> 12) & 0x3f);
                           *to++ = 0x80 + ((ucode >> 6) & 0x3f);
                           *to++ = 0x80 + (ucode & 0x3f);
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

static const char *echttp_json_value (JsonContext context) {

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

static const char *echttp_json_array (JsonContext context) {

    const char *error = 0;
    int i = context->count;
    JsonToken *token = context->token;
    char *json = context->json;

    token[i].type = JSON_ARRAY;
    token[i].length = 0;

    JSONTRACE ("array");
    for (;;) {
        error = add_token(context);
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

static const char *echttp_json_object (JsonContext context) {

    const char *error = 0;
    int i = context->count;
    JsonToken *token = context->token;
    char *json = context->json;

    token[i].type = JSON_OBJECT;
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

        error = add_token(context);
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

void echttp_json_enable_debug (void) {
    echttp_json_debug = 1;
}

const char *echttp_json_parse (char *json, JsonToken *token, int *count) {

   const char *error;
   struct JsonContext_s context;

   if (!count || !*count) return "invalid count parameter";

   context.line_count = 1;
   context.line_start = 0;
   context.cursor = context.count = 0;

   context.json = json;
   context.token = token;
   context.max = *count;

   echttp_json_error_text[0] = 0;

   token[0].key = 0;

   switch (skip_spaces (&context)) {
       case 0:   error = "no data"; break;
       case '{': error = echttp_json_object (&context); break;
       case '[': error = echttp_json_array(&context); break;
       default:  error = echttp_json_value (&context);
   }

   if (! error) {
       if (next_word(&context) != 0) error = "data left at the end of input";
   }

   if (error) {
       snprintf (echttp_json_error_text, sizeof(echttp_json_error_text),
                 "%s at line %d, column %d\n",
                 error, context.line_count, context.cursor-context.line_start);
       return echttp_json_error_text;
   }
   *count = context.count + 1;
   return 0;
}


static void echttp_json_gen_append (JsonContext context, const char *text) {

    if (context->cursor < context->size) {
        strncpy (context->json+context->cursor,
                 text, context->size-context->cursor);
        context->json[context->size-1] = 0;
        context->cursor += strlen(context->json+context->cursor);
    }
}

static void echttp_json_gen_indent (JsonContext context) {

    if (context->options & JSON_OPTION_PRETTY) {
        static char indent[81];
        if (indent[0] == 0) memset (indent, ' ',sizeof(indent)-1);
        const char *sp = indent + sizeof(indent) -1 - (4 * context->depth);
        echttp_json_gen_append (context, sp);
    }
}

static void echttp_json_gen_key (JsonContext context, const char *key) {
    echttp_json_gen_indent (context);
    if (key) {
        const char *sep = (context->options & JSON_OPTION_PRETTY)?"\" : ":"\":";
        echttp_json_gen_append (context, "\"");
        echttp_json_gen_append (context, key);
        echttp_json_gen_append (context, sep);
    }
}

static void echttp_json_gen_eol (JsonContext context, int comma) {
    if (context->options & JSON_OPTION_PRETTY) {
        if (comma)
            echttp_json_gen_append (context, ",\n");
        else
            echttp_json_gen_append (context, "\n");
    } else if (comma) {
        echttp_json_gen_append (context, ",");
    }
}

static void echttp_json_gen_bool (JsonContext context, int i) {
    echttp_json_gen_append (context,
                            context->token[i].value.bool?"true":"false");
}

static void echttp_json_gen_integer (JsonContext context, int i) {
    char buffer[32];
    snprintf (buffer, sizeof(buffer), "%d", context->token[i].value.integer);
    echttp_json_gen_append (context, buffer);
}

static void echttp_json_gen_real (JsonContext context, int i) {
    char buffer[64];
    snprintf (buffer, sizeof(buffer), "%e", context->token[i].value.real);
    echttp_json_gen_append (context, buffer);
}

static void echttp_json_gen_string (JsonContext context, int i) {

    static char escapelist [128];
    static char tohex [16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

    const char *value = context->token[i].value.string;
    char *buffer = (char *)malloc (3*strlen(value) + 3); // Worst case.
    char *to = buffer;

    if (escapelist['n'] == 0) {
        escapelist['\"'] = '\"';
        escapelist['\\'] = '\\';
        escapelist['/'] = '/';
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
        } else if ((int)(*value) & 0xff >= 0xf1) {
            // Invalid UTF8 character, ignore.
            value += 1;
        } else {
            int ucode;
            *to++ = '\\';
            *to++ = 'u';
            if ((int)(*value) & 0xff < 0xe0) {
                // UTF-16 (2 bytes).
                ucode = ((int)(value[0]) & 0x1f) << 6 +
                        ((int)(value[1]) & 0x3f);
                *to++ = tohex[((ucode >> 12) & 0x0f)];
                *to++ = tohex[((ucode >> 8) & 0x0f)];
                *to++ = tohex[((ucode >> 4) & 0x0f)];
                *to++ = tohex[(ucode & 0x0f)];
                value += (value[1] < 0) ? 2 : 1;
            } else if ((int)(*value) & 0xff < 0xf0) {
                // UTF-16 (3 bytes).
                ucode = ((int)(value[0]) & 0x0f) << 12 +
                        ((int)(value[1]) & 0x3f) << 6 +
                        ((int)(value[2]) & 0x3f);
                *to++ = tohex[((ucode >> 12) & 0x0f)];
                *to++ = tohex[((ucode >> 8) & 0x0f)];
                *to++ = tohex[((ucode >> 4) & 0x0f)];
                *to++ = tohex[(ucode & 0x0f)];
                value += (value[1] < 0) ? 2 : 1;
                if (value[2] < 0) value += 1;
            } else {
                // UTF-16 surrogate pair (4 bytes).
                int incr = 1;
                int pair;
                ucode = (((int)(value[0]) & 0xf) << 18) +
                        (((int)(value[1]) & 0x3f) << 12) +
                        (((int)(value[2]) & 0x3f) << 6) +
                        ((int)(value[3]) & 0x3f) - 0x10000;
                pair = 0xd800 + (ucode >> 10);
                *to++ = tohex[(pair >> 12) & 0x0f];
                *to++ = tohex[(pair >> 8) & 0x0f];
                *to++ = tohex[(pair >> 4) & 0x0f];
                *to++ = tohex[pair & 0x0f];
                *to++ = '\\';
                *to++ = 'u';
                pair = 0xdc00 + (ucode & 0x3ff);
                *to++ = tohex[(pair >> 12) & 0x0f];
                *to++ = tohex[(pair >> 8) & 0x0f];
                *to++ = tohex[(pair >> 4) & 0x0f];
                *to++ = tohex[pair & 0x0f];
                if (value[1] < 0) {
                    incr += 1;
                    if (value[2] < 0) {
                        incr += 1;
                        if (value[3] < 0) incr += 1;
                    }
                }
                value += incr;
            }
        }
    }
    *to++ = '"';
    *to = 0;
    echttp_json_gen_append (context, buffer);
    free(buffer);
}

const char *echttp_json_generate (JsonToken *token, int count,
                                  char *json, int size, int options) {

    struct JsonContext_s context;

    int i;

    context.json = json;
    context.size = size;
    context.token = token;
    context.max = context.count = count;
    context.options = options;

    context.cursor = 0;
    context.countdown[0] = 0;

    for (i = 0; i < count; ++i) {

        int comma = context.countdown[context.depth] > 1;

        echttp_json_gen_key (&context, token[i].key);

        switch (token[i].type) {
            case JSON_NULL:
                echttp_json_gen_append (&context, "null"); break;
            case JSON_BOOL:
                echttp_json_gen_bool (&context, token[i].value.bool); break;
            case JSON_INTEGER:
                echttp_json_gen_integer (&context, i); break;
            case JSON_REAL:
                echttp_json_gen_real (&context, i); break;
            case JSON_STRING:
                echttp_json_gen_string (&context, i); break;
            case JSON_ARRAY:
                echttp_json_gen_append (&context, "[");
                context.depth += 1;
                context.ending[context.depth] = ']';
                context.countdown[context.depth] = token[i].length + 1;
                comma = 0;
                break;
            case JSON_OBJECT:
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

static int search_array_element (const JsonToken *token,
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
            if (depth >= JSON_MAX_DEPTH) return -1;
            stack[++depth] = token[i].length + 1;
        }
    }
    return -1;
}

static int search_object_element (const JsonToken *parent, const char *path) {

    const char *p = next_separator(path);
    int length = (int) (p - path);
    int depth = 0;
    int stack[JSON_MAX_DEPTH];
    int i;
    int count = parent->length;

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
                int d = echttp_json_search (parent+i, p);
                if (d < 0) return -1;
                return i + d;
            }
        }
        if (parent[i].length > 0) {
            count += parent[i].length;
            if (depth >= JSON_MAX_DEPTH) return -1;
            stack[++depth] = parent[i].length + 1;
        }
    }
    return -1;
}

int echttp_json_search (const JsonToken *parent, const char *path) {

    if (*path == 0) return 0; // End of search: any type is fine.

    if (*path == '.' && parent->type == JSON_OBJECT) {
        return search_object_element (parent, path+1);
    }

    if (*path == '[' && parent->type == JSON_ARRAY) {
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

const char *echttp_json_enumerate (const JsonToken *parent, int *index) {

    int depth = 0;
    int stack[JSON_MAX_DEPTH];
    int i;
    int child = 0;
    int count = parent->length;

    if (count == 0) {
        if (parent->type != JSON_ARRAY && parent->type != JSON_OBJECT)
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
            if (depth >= JSON_MAX_DEPTH) return "data structure too deep";
            stack[++depth] = parent[i].length + 1;
        }
    }
    if (child != parent->length) return "too few items found";

    return 0;
}

