/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_json.h - An additional module to decode JSON text,
 */

#include "echttp_parser.h"

void echttp_json_enable_debug (void);

int echttp_json_estimate (const char *json);
const char *echttp_json_parse (char *json, ParserToken *token, int *count);

int echttp_json_search (const ParserToken *parent, const char *path);

const char *echttp_json_enumerate (const ParserToken *parent, int *index);

ParserContext echttp_json_start
                  (ParserToken *token, int max, char *pool, int size);
void echttp_json_add_null
         (ParserContext context, int parent, const char *key);
void echttp_json_add_bool
         (ParserContext context, int parent, const char *key, int value);
void echttp_json_add_integer
         (ParserContext context, int parent, const char *key, long long value);
void echttp_json_add_real
         (ParserContext context, int parent, const char *key, double value);
void echttp_json_add_string
         (ParserContext context, int parent, const char *key, const char *value);
int echttp_json_add_object
         (ParserContext context, int parent, const char *key);
int echttp_json_add_array
         (ParserContext context, int parent, const char *key);
int echttp_json_end (ParserContext context);

const char *echttp_json_format (ParserToken *token, int count,
                                char *json, int size, int options);

const char *echttp_json_export (ParserContext context, char *buffer, int size);

