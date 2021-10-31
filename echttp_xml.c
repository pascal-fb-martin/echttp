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
 * A minimal XML decoder design for simple interface and minimal use
 * of resources.
 *
 * This XML decoder was inpired by JSMN (https://github.com/zserge/jsmn),
 * but is a totally independent implementation, using recursive descent
 * instead of a state machine.
 *
 * int echttp_xml_estimate (const char *xml);
 *
 * const char *echttp_xml_parse (char *xml, ParserToken *token, int *count);
 *
 *    Decode a XML string and return a list of tokens. The decoding breaks
 *    the input string.
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "echttp.h"
#include "echttp_xml.h"

/* This data structure holds the current state of the parser. It is meant
 * to make it easy to pass the current content from one level to the next.
 */
struct ParserContext_s {
    int cursor;
    int count;

    int line_count;
    int line_start;
    char *error;

    char *xml;
    ParserToken *token;
    int max;
};

static int  echttp_xml_debug = 0;
static char echttp_xml_error_text[160];

#define XMLTRACE(x) if (echttp_xml_debug) printf ("%s at line %d column %d: %10.10s\n", x, context->line_count, context->cursor - context->line_start + 1, context->xml + context->cursor);

static char skip_spaces (ParserContext context) {

    char *xml = context->xml;

    while (isspace(xml[context->cursor])) {
        if (xml[context->cursor] == '\n') {
            context->line_count += 1;
            context->line_start = context->cursor + 1;
        }
        context->cursor += 1;
    }
    XMLTRACE ("next word");
    return xml[context->cursor];
}

static char next_word (ParserContext context) {
    context->cursor += 1;
    return skip_spaces (context);
}

static const char *add_token (ParserContext context, int type) {
    context->token[context->count].type = type;
    context->token[context->count].length = 0;
    context->token[context->count].value.string = 0;
    context->count += 1;
    if (context->count > context->max) return "XML structure is too long";
    return 0;
}

void echttp_xml_enable_debug (void) {
    echttp_xml_debug = 1;
}

static const char *echttp_xml_element (ParserContext context, int index);

static const char *echttp_xml_string (ParserContext context, int parent) {

    char *from = context->xml + context->cursor;
    char *to = from;
    int quoted = (*from == '"');
    int string = context->count;

    if (quoted) to = ++from;

    const char *error = add_token (context, PARSER_STRING);
    context->token[string].value.string = to;

    if (parent >= 0) context->token[parent].length += 1;

    XMLTRACE ("string");
    for (;;) {
        switch (*from) {
            case '&':
                if (strncmp (from+1, "amp;", 4) == 0) {
                    *to++ = '&';
                    from += 5;
                } else if (strncmp (from+1, "lt;", 3) == 0) {
                    *to++ = '<';
                    from += 4;
                } else if (strncmp (from+1, "gt;", 3) == 0) {
                    *to++ = '>';
                    from += 4;
                } else if (strncmp (from+1, "quot;", 5) == 0) {
                    *to++ = '\"';
                    from += 6;
                } else if (strncmp (from+1, "apos;", 5) == 0) {
                    *to++ = '\'';
                    from += 6;
                } else {
                    return "unsupported XML escape";
                }
                break;
            case '<':
                if (quoted) return "invalid tag in a quoted string";
                if (to != from) *to = 0;
                context->cursor = (int)(from - context->xml);
                XMLTRACE("end string");
                return 0;
            case 0:
                return "unterminated string";
            case '\"':
                if (quoted) {
                    *to = 0;
                    context->cursor = (int)(from - context->xml) + 1;
                    XMLTRACE("end quoted string");
                    return 0;
                }
                // Continue one as a normal character.
            default:
                if (to != from) *to = *from;
                to++; from++;
        }
    }

    return "unterminated string";
}

static const char *echttp_xml_content (ParserContext context, int parent) {
    XMLTRACE ("content");
    const char *error;
    int content = context->count;
    if (skip_spaces(context) == '<') {
        int first = 1;
        do {
            if (context->xml[context->cursor+1] == '/') break;
            if (first) {
                error = add_token (context, PARSER_OBJECT);
                if (error) return error;
                context->token[parent].length += 1;
                first = 0;
            }
            context->cursor += 1;
            error = echttp_xml_element (context, content);
            if (error) return error;
        } while (skip_spaces(context) == '<');
    } else {
        error = echttp_xml_string (context, parent);
        if (error) return error;
        if (strncmp (context->xml+context->cursor, "</", 2))
            return "invalid XML tag end";
        context->xml[context->cursor] = 0;
    }
    context->token[content].key = "content";

    context->cursor+= 2;
    const char *key = context->token[parent].key;
    int keylen = strlen(key);
    if (strncmp (context->xml+context->cursor, key, keylen))
        return "unmatched end tag name";
    if (context->xml[context->cursor+keylen] != '>')
        return "invalid tag end syntax";
    context->cursor += keylen + 1;
    return 0;
}

static const char *echttp_xml_tagname (ParserContext context, int index) {

    XMLTRACE ("tag");
    char *xml = context->xml;
    context->token[index].key = xml + context->cursor;
    while (xml[context->cursor] != 0 &&
            (!isspace(xml[context->cursor])) &&
            xml[context->cursor] != '/' && xml[context->cursor] != '>')
        context->cursor += 1;
    return 0;
}

static int echttp_xml_valid_attribute (char c) {
    return isalnum(c) || c == '_' || c == ':' || c == '-';
}

static const char *echttp_xml_attributes (ParserContext context, int parent) {
    char *xml = context->xml;
    ParserToken *token = context->token;
    int attributes = context->count;

    XMLTRACE ("attributes");
    const char *error = add_token (context, PARSER_OBJECT);
    token[attributes].key = "attributes";
    if (error) return error;
    token[parent].length += 1;

    for (;;) {
        switch (skip_spaces(context)) {
            case 0: return "unterminated tag";
            case '/': return 0;
            case '>': return 0;
        }
        XMLTRACE ("attribute");
        if (!isalpha(xml[context->cursor])) return "invalid attributes name";

        int this_attribute = context->count;
        token[this_attribute].key = xml + context->cursor;
        do {
            context->cursor += 1;
        } while (echttp_xml_valid_attribute(xml[context->cursor]));
        if (xml[context->cursor] != '=') return "invalid attributes syntax";
        xml[context->cursor] = 0;
        context->cursor += 1;

        if (xml[context->cursor] != '"') return "unquoted attribute value";
        error = echttp_xml_string (context, attributes);
        if (error) return error;
    }
    return 0;
}

static const char *echttp_xml_skip_comments (ParserContext context) {

    // Skip any XML comment or CDATA.
    char *xml = context->xml;

    while (xml[context->cursor] == '!') {

        XMLTRACE ("comment");
        if (xml[context->cursor+1] == '-' && xml[context->cursor+2] == '-') {
            char *p = strstr(xml+context->cursor+3, "-->");
            if (!p) return "unterminated XML comment";
            context->cursor = p - xml + 3;
        } else if (strncmp (xml+context->cursor, "![CDATA[", 8) == 0) {
            char *p = strstr(xml+context->cursor+8, "]]>");
            if (!p) return "unterminated XML CDATA";
            context->cursor = p - xml + 3;
        } else {
            return "invalid XML section";
        }
        if (skip_spaces(context) != '<') return "invalid XML element";
        context->cursor += 1;
    }
    return 0;
}

static const char *echttp_xml_element (ParserContext context, int parent) {

    char *xml = context->xml;
    int index = context->count;

    XMLTRACE ("element");
    const char *error = add_token (context, PARSER_OBJECT);
    if (error) return error;
    context->token[parent].length += 1;

    error = echttp_xml_skip_comments (context);
    if (error) return error;

    error = echttp_xml_tagname (context, index);
    if (error) return error;

    if (xml[context->cursor] == ' ') {
        xml[context->cursor] = 0;
        context->cursor += 1;
        skip_spaces (context);
        error = echttp_xml_attributes (context, index);
        if (error) return error;
    }
    switch (xml[context->cursor]) {
        case '/':
            if (xml[context->cursor+1] != '>')
                return "Invalid tag end";
            xml[context->cursor] = 0;
            context->cursor += 2;
            return 0; // No content.
        case '>':
            xml[context->cursor] = 0;
            context->cursor += 1;
            break;
        default:
            return "invalid tag character";
    }
    return echttp_xml_content (context, index);
}

int echttp_xml_estimate (const char *xml) {
    int count = 1; // Implicit outer object.
    // This method of counting does not escape the literal strings content,
    // always assumes a token for attributes and accounts for 4 items per
    // start/end tag pair instead of the max 3.
    // This is not a problem because the goal is to estimate a sufficient
    // space, not the smallest possible space.
    for (;;) {
        switch (*(xml++)) {
            case '=': // Tell-all sign of an attribute.
                count += 1;
                break;
            case '>':
                count += 2; // total of 4 when content is present (start + end)
                break;
            case 0:
                return count;
        }
    }
}

const char *echttp_xml_parse (char *xml, ParserToken *token, int *count) {

   const char *error;
   struct ParserContext_s context;

   if (!count || !*count) return "invalid count parameter";

   context.line_count = 1;
   context.line_start = 0;
   context.cursor = context.count = 0;

   context.xml = xml;
   context.token = token;
   context.max = *count;

   echttp_xml_error_text[0] = 0;
   *count = 0;

   token[0].key = 0;

   switch (skip_spaces (&context)) {
       case 0:   error = "no data"; break;
       case '<':
          context.cursor += 1;
          while (xml[context.cursor] == '?' || xml[context.cursor] == '!') {
              // Skip any XML declaration, processing instruction and comment.
              if (xml[context.cursor] == '?') {
                  char *p = strstr(xml+context.cursor+1, "?>");
                  if (!p) return "unterminated XML processing instruction";
                  context.cursor = p - xml + 3;
              } else if (strncmp (xml+context.cursor, "!--", 3) == 0) {
                  char *p = strstr(xml+context.cursor+4, "-->");
                  if (!p) return "unterminated XML comment";
                  context.cursor = p - xml + 3;
              } else if (strncmp (xml+context.cursor, "![CDATA[", 8) == 0) {
                  char *p = strstr(xml+context.cursor+8, "]]>");
                  if (!p) return "unterminated XML CDATA";
                  context.cursor = p - xml + 3;
              } else if (strncmp (xml+context.cursor, "!DOCTYPE", 8) == 0) {
                  char *p = strchr(xml+context.cursor+8, '>');
                  if (!p) return "unterminated XML DOCTYPE";
                  context.cursor = p - xml + 1;
              } else
                  return "invalid XML section";

              skip_spaces(&context);
              if (xml[context.cursor] != '<') return "invalid XML content";
              context.cursor += 1;
          }
          add_token (&context, PARSER_OBJECT);
          error = echttp_xml_element (&context, 0);
          break;
       default:  return "probably not XML data";
   }
   *count = context.count;

   if (! error) {
       if (next_word(&context) != 0) error = "data left at the end of input";
   }

   if (error) {
       snprintf (echttp_xml_error_text, sizeof(echttp_xml_error_text),
                 "%s at line %d, column %d\n",
                 error, context.line_count, context.cursor-context.line_start);
       return echttp_xml_error_text;
   }
   return 0;
}

