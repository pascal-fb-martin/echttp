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
 * int echttp_json_search (const JsonToken *token, int max, const char *id);
 *
 *    retrieve a JSON item (after decoding).
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "echttp.h"
#include "echttp_json.h"


/* This data structure holds the current state of the parser. It is meant
 * to make it easy to pass the current content from one level to the next.
 */
typedef struct {
    int cursor;
    int count;

    int line_count;
    int line_start;
    char *error;

    char *json;
    JsonToken *token;
    int max;
} JsonContext;

static int  echttp_json_debug = 0;
static char echttp_json_error_text[160];

static const char *echttp_json_object (JsonContext *context);
static const char *echttp_json_array (JsonContext *context);

#define JSONTRACE(x) if (echttp_json_debug) printf ("%s at line %d column %d: %s\n", x, context->line_count, context->cursor - context->line_start + 1, context->json + context->cursor);

static char skip_spaces (JsonContext *context) {

    char *json = context->json;

    while (isspace(json[context->cursor])) {
        if (json[context->cursor] == '\n') {
            context->line_count += 1;
            context->line_start = context->cursor + 1;
        }
        context->cursor += 1;
    }
    JSONTRACE ("next word");
    return json[context->cursor];
}

static char next_word (JsonContext *context) {
    context->cursor += 1;
    return skip_spaces (context);
}

static const char *add_token (JsonContext *context) {
    context->count += 1;
    if (context->count > context->max) return "JSON structure is too long";
    return 0;
}

static const char *echttp_json_literal (JsonContext *context) {

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

static const char *echttp_json_number (JsonContext *context) {

    char *json = context->json + context->cursor;

    JSONTRACE ("number");
    while (isdigit(*json)) json += 1;
    switch (*json) {
        case '.': case 'e': case 'E':
            context->token[context->count].type = JSON_REAL;
            context->token[context->count].value.real =
                strtof(context->json + context->cursor, &json);
            context->cursor = (int) (json - context->json) - 1;
            break;
        default:
            context->token[context->count].type = JSON_INTEGER;
            context->token[context->count].value.integer =
                strtol(context->json + context->cursor, &json, 0);
            context->cursor = (int) (json - context->json) - 1;
            break;
    }
    context->token[context->count].length = 0;
    return 0;
}

static char hex2bin(char x) {
    if (x >= '0' && x <= '9') return x - '0';
    if (x >= 'a' && x <= 'f') return x - 'a' + 10;
    if (x >= 'A' && x <= 'F') return x - 'A' + 10;
    return -1;
}

static const char *echttp_json_string (JsonContext *context) {
    char *from = context->json + context->cursor + 1;
    char *to = from;
    int l, h;

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
                       if (l > 0 || h > 0) *to++ = 16 * h + l;
                       h = hex2bin(from[3]);
                       l = hex2bin(from[4]);
                       if (h < 0 || l < 0) return "invalid unicode";
                       *to++ = 16 * h + l;
                       from += 4;
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

static const char *echttp_json_value (JsonContext *context) {

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
            error = echttp_json_number (context);
            break;

        default:
            return "invalid character, expected a value";
    }
    return error;
}

static const char *echttp_json_array (JsonContext *context) {

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

static const char *echttp_json_object (JsonContext *context) {

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
   JsonContext context;

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


static const char *next_separator (const char *p) {
    while (*p) {
        if (*p == '.') return p;
        if (*p == '[') return p;
        p++;
    }
    return p;
}

static int move_to_array_element (const JsonToken *token, int count,
                                  const char *id, int index);

static int apply_match (const JsonToken *token, int count, const char *id) {

    if (*id == 0) return 0; // End of search: any type is fine.

    if (*id == '.' && token->type == JSON_OBJECT) {
        int d = echttp_json_search (token+1, count-1, id+1);
        if (d < 0) return -1;
        return d + 1;
    }

    if (*id == '[' && token->type == JSON_ARRAY) {
        char *end;
        int d;
        int i = strtol(id+1, &end, 0);
        if (*end != ']' || i < 0 || i >= token->length) return -1;
        d = move_to_array_element (token+1, count-1, end+1, i);
        if (d < 0) return -1;
        return d + 1;
    }
    return -1; // Not the type of token we expected.
}

static int move_to_array_element (const JsonToken *token,
                                  int count, const char *id, int index) {
    int depth = 0;
    int stack[32];
    int i;

    for (i = 0; i < count; i++) {

        // Skip deeper elements.

        while (depth > 0) {
            stack[depth] -= 1;
            if (stack[depth] > 0) break;
            depth -= 1;
        }
        if (depth > 0) continue;

        if (index == 0) {
            int d = apply_match (token+i, count-i, id);
            if (d < 0) return -1;
            return i + d;
        }
        if (token[i].length > 0) stack[++depth] = token[i].length + 1;
        index--;
    }
}

int echttp_json_search (const JsonToken *token, int count, const char *id) {

    const char *p = next_separator(id);
    int length = (int) (p - id);
    int depth = 0;
    int stack[32];
    int match;
    int i;

    for (i = 0; i < count; i++) {

        // Skip deeper elements.

        while (depth > 0) {
            stack[depth] -= 1;
            if (stack[depth] > 0) break;
            depth -= 1;
        }
        if (depth > 0) continue;

        match = 0;
        if (token[i].key && length) {
            match = (strncmp (id, token[i].key, length) == 0);
        } else if (token[i].key == 0 && length == 0) {
            match = 1;
        }
        if (match) {
            int d = apply_match (token+i, count-i, p);
            if (d < 0) return -1;
            return i + d;
        }
        if (token[i].length > 0) stack[++depth] = token[i].length + 1;
    }

    return -1;
}

