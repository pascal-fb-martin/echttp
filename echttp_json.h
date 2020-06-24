/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_json.h - An additional module to decode JSON text,
 */

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
} JsonToken;

// JSON types:
#define JSON_NULL    1
#define JSON_BOOL    2
#define JSON_INTEGER 3
#define JSON_REAL    4
#define JSON_STRING  5
#define JSON_ARRAY   6
#define JSON_OBJECT  7

void echttp_json_enable_debug (void);

const char *echttp_json_parse (char *json, JsonToken *token, int *count);

int echttp_json_search (const JsonToken *parent, const char *path);

const char *echttp_json_enumerate (const JsonToken *parent, int *index);

