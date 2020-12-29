/* echttp - Embedded HTTP server.
 *
 * A minimal HTTP server library designed for simplicity and embedding in
 * existing applications.
 *
 * echttp_xml.h - An additional module to decode XML text,
 */

#include "echttp_parser.h"

void echttp_xml_enable_debug (void);

int echttp_xml_estimate (const char *xml);
const char *echttp_xml_parse (char *xml, ParserToken *token, int *count);

