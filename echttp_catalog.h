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
    const char *value;
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
} echttp_catalog;

unsigned int echttp_catalog_signature (const char *name);

int echttp_catalog_find (echttp_catalog *d, const char *name);

void echttp_catalog_reset (echttp_catalog *d);

void echttp_catalog_set (echttp_catalog *d,
                         const char *name, const char *value);

const char *echttp_catalog_refresh
               (echttp_catalog *d, const char *name, const char *value, time_t timestamp);

const char *echttp_catalog_get (echttp_catalog *d, const char *name);

void echttp_catalog_join (echttp_catalog *d,
                          const char *sep, char *text, int size);

