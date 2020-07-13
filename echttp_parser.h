/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_json.h - An additional module to decode JSON text,
 */

#ifndef INCLUDED__ECHTTP_PARSER__H
#define INCLUDED__ECHTTP_PARSER__H

typedef struct {
    const char *key;
    int type;
    union {
        int bool;
        long integer;
        double real;
        char *string;
    } value;
    int length;
} ParserToken;

// Basic types:
#define PARSER_NULL    1
#define PARSER_BOOL    2
#define PARSER_INTEGER 3
#define PARSER_REAL    4
#define PARSER_STRING  5
#define PARSER_ARRAY   6
#define PARSER_OBJECT  7

// Code generation options:

#define PRINT_OPTION_PRETTY   1

struct ParserContext_s;
typedef struct ParserContext_s *ParserContext;

#endif // INCLUDED__ECHTTP_PARSER__H

