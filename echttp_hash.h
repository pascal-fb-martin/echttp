/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.

 * This module implements the mechanism used to map names to values. This is
 * used for query parameters, HTTP attributes, etc.
 */

#include "time.h"

typedef struct {
    const char *name;
    void       *value;
    unsigned int signature;
    time_t       timestamp;
    int next;
} echttp_symbol;

#define ECHTTP_HASH 127
#define ECHTTP_MAX_SYMBOL 256

typedef struct {
    int count;
    int index[ECHTTP_HASH];
    echttp_symbol item[ECHTTP_MAX_SYMBOL];
} echttp_hash;

typedef int echttp_hash_action (int i, const char *name);

unsigned int echttp_hash_signature (const char *name);

void echttp_hash_reset (echttp_hash *d, echttp_hash_action *action);

int echttp_hash_find (echttp_hash *d, const char *name);
int echttp_hash_next (echttp_hash *d, int from, const char *name);
int echttp_hash_add (echttp_hash *d, const char *name);

int echttp_hash_insert (echttp_hash *d, const char *name);

int echttp_hash_iterate (echttp_hash *d,
                         const char *name, echttp_hash_action *action);

// Compatibility API, to be phased out.
//
void echttp_hash_set (echttp_hash *d, const char *name, void *value);

void *echttp_hash_refresh
          (echttp_hash *d, const char *name, void *value, time_t timestamp);

void *echttp_hash_get (echttp_hash *d, const char *name);

