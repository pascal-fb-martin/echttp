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

// JSON generation options:

#define JSON_OPTION_PRETTY   1

struct JsonContext_s;
typedef struct JsonContext_s *JsonContext;

void echttp_json_enable_debug (void);

const char *echttp_json_parse (char *json, JsonToken *token, int *count);

int echttp_json_search (const JsonToken *parent, const char *path);

const char *echttp_json_enumerate (const JsonToken *parent, int *index);

JsonContext echttp_json_start
                (JsonToken *token, int max, char *pool, int size);
void echttp_json_add_null
         (JsonContext context, int parent, const char *key);
void echttp_json_add_bool
         (JsonContext context, int parent, const char *key, int value);
void echttp_json_add_integer
         (JsonContext context, int parent, const char *key, long value);
void echttp_json_add_real
         (JsonContext context, int parent, const char *key, double value);
void echttp_json_add_string
         (JsonContext context, int parent, const char *key, const char *value);
int echttp_json_add_object
         (JsonContext context, int parent, const char *key);
int echttp_json_add_array
         (JsonContext context, int parent, const char *key);
int echttp_json_end (JsonContext context);

const char *echttp_json_generate (JsonToken *token, int count,
                                  char *json, int size, int options);

const char *echttp_json_format (JsonContext context, char *buffer, int size);

