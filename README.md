# Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed with a simple API in plain C and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely, or that exports (or is the client of) a web API. Typically this application would run on a Raspberry Pi or an old mini PC acting as a home server, running Linux.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)

After initializing the HTTP server, and then completing its own initialization, the application reacts to file descriptor events and HTTP requests as provided by the HTTP server.

This web server can be configured to use dynamic port allocation for its server socket. Before you hit your head against the proverbial wall trying to understand why the hell this is even allowed, please take a look at [houseportal](https://github.com/pascal-fb-martin/houseportal).

# Installation

* Clone the repository.
* cd echttp
* make
* sudo make install (this installs files in the /usr/local tree)

Use the OpenSSL and -lechttp options when building your application. For example:
```
cc -o httpserver httpserver.c -lechttp -lssl -lcrypto
```
# Example
Below is an example of a small, but fully functional web server written using echttp. This server returns pages from the current directory as well as an embedded welcome message.
```
#include <stdio.h>
#include <unistd.h>

#include "echttp.h"
#include "echttp_static.h"

static const char *welcome (const char *method, const char *uri,
                            const char *data, int length) {
    static char buffer[1024];
    const char *host = echttp_attribute_get("Host");
    const char *who = echttp_parameter_get("who");
    if (host == 0) host = "(unknown)";
    if (who == 0) who = "Anonymous";
    echttp_content_type_html ();
    snprintf (buffer, sizeof(buffer),
              "<e>You are welcome on <b>%s</b>, dear <i>%s</i>!</e>", host, who);
    return buffer;
}

int main (int argc, const char **argv) {
    if (echttp_open (argc, argv) >= 0) {
        echttp_route_uri ("/welcome", welcome);
        echttp_static_route ("/static", getcwd(0, 0));
        echttp_loop();
    }
    return 1;
}
```
# API
## Base HTTP Server
The application must include echttp.h as a prerequisit to using the echttp API.

The echttp primitives are:
```
const char *echttp_help (int level);
```
Return a help text to print. If level is 0, it returns the short list of options in a single line, otherwise it returns the detailed description for option N. To print the detailed descriptions for all options, the caller should loop starting at 1 and stop when the return is 0 (null).

```
void echttp_default (const char *arg);
```
Override one HTTP server hard-coded default. This function must be called before echttp_open(). It allows the application to define its own defaults. For example an application might want to use "dynamic" as its default TCP service name, while still allowing the user to force some specific port number instead. The argument must be statically allocated. This function may be called multiple times.

```
int echttp_open (int argc, const char **argv);
```
Initialize the HTTP server. The HTTP-specific arguments are removed from the argument list and the count of remaining arguments is returned.

The arguments consumed by echttp_open are:

* -http-service=_name_ (Service name or port number to listen to; default is "http", i.e. port 80. A dynamic port number will be used if the service name is "dynamic".)
* -http-debug (If present, expect to see a lot of debug traces.)

```
typedef const char *echttp_callback (const char *method, const char *uri,
                                     const char *data, int length);
```
The profile for any HTTP request processing function. The string returned contains the data to send back to the client; it must not be a local (stack) variable, since it is used after the callback returned.

```
int echttp_route_uri (const char *uri, echttp_callback *call);
```
Define a route for processing the exact specified URI. Return the route descriptor or -1 on failure.

The callback declared here is called only when all the content data has been received.
```
int echttp_route_match (const char *root, echttp_callback *call);
```
Defines a route for a parent URI and all its children. Return the route descriptor or -1 on failure.

The callback declared here is called only when all the content data has been received.
```
void echttp_route_remove (const char *uri);
```
Remove a previously declared route, either an exact route or a root match.
The caller is responsible for deallocating the uri (or root) string, if
necessary.
```
typedef void echttp_protect_callback (const char *method, const char *uri);
int echttp_protect (int route, echttp_protect_callback *call);
```
Declare a protect callback for the specified route. The route descriptor is the value returned by echttp_route_uri() or echttp_route_match(). A protect callback is called before the route's callback and may change the HTTP status to not OK (by calling echttp_error()--see later) and set the response's attributes; in that case the route's callback function is not called.

If the route is 0 the protect callback applies to all routes, in addition of each route protect callback. Thus the application may define two level of protect callback: one for all requests, and one for each route.

Note that the protect callback is associated with a route, i.e. the URI string, not with a callback function: one may define two routes to the same callback function, and protect one route but not the other. This is intentional, as this allows protecting specific file paths without protecting all file paths (see the static page extention later).

The global protect (route 0) is called first, then the protect for the specific route. However the later is not called if the global protect changed the HTTP status to something different than 200. How the HTTP request is then processed depends on the HTTP status after both protect calls:

* If the status is not a 2xx, the route callback is not called and the connection is closed after the response has been sent.
* If the status is 204, the route callback is not called and no data is returned with the response.
* If the status is any other 2xx, the route callback is called.

This mechanism is meant to facilitates the implementation of access control extensions, this is not an access control method on its own.

This function returns the original route descriptor (to allow to chain calls) or -1 on failure.

```
int echttp_asynchronous_route (int route, echttp_callback *call);
```
Declare an asynchronous callback for the specified route (see echttp_callback above). Return the original route descriptor, which allows to chain calls, or -1 on failure.

The asynchronous route mode is intended for PUT or POST requests that transfer large amounts of data, like image or video.

You cannot declare route 0 (i.e. all routes) as asynchronous, as this could impact routes that never expected to be called in asynchronous mode.

You can change a route back to the default (synchronous) mode by assigning a null callback. You can also change the callback function. This take effect immediately, including for pending requests. This is not necessarily recommended..

The asynchronous callback is different from the normal callback in that it is called only if not all content data has been received. The provided data is the portion of the content that _was_ received. The code should compare the value of Content-Length with the length argument to retrieve how much content is still expected. The function _must_ save all received content, call echttp_transfer() to start the transfer of the remaining content and then return no data (null pointer).

If the asynchronous callback function does not call echttp_transfer() and does not trigger an error or a redirect, the pending request reverts to synchronous mode.

The request processing stops if the asynchronous callback triggers a redirect or an error: the HTTP response is immediately sent and the remaining content is ignored.

Otherwise, the normal (synchronous) callback will be called, with no data, once the full content has been transfered. This callback may retrieve the length of the content through the Content-Length attribute. (If Content-Length is 0, the request has no content.)

```
const char *echttp_attribute_get (const char *name); 
```
Retrieve the value of the specified HTTP attribute, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.
```
const char *echttp_parameter_get  (const char *name);
```
Retrieve the value of the specified HTTP parameter, or 0 if not found. This function should be called from within an HTTP callback, while processing an HTTP request.

Parameter names are case insensitive: __NaMe__ is the same as __name__.
```
void echttp_parameter_join (char *text, int size);
```
Retrieve all HTTP parameters and rebuild the HTTP parameter string. This function should be called from within an HTTP callback, while processing an HTTP request.
```
void echttp_attribute_set (const char *name, const char *value);
```
Set an attribute for the HTTP response. This function should be called from within an HTTP callback, while processing an HTTP request. The value must not be a local variable (it must be static) because it will be used after the HTTP callback returns.
```
void echttp_content_type_set (const char *value);
```
A shorthand for setting the Content-Type attribute.
```
void echttp_content_type_html (void);
```
A shorthand for setting the Content-Type attribute to HTML.
```
void echttp_content_type_json (void);
```
A shorthand for setting the Content-Type attribute to JSON.
```
void echttp_content_type_css (void);
```
A shorthand for setting the Content-Type attribute to CSS.
```
void echttp_content_length (int length);
```
Force the length of the data that will be returned. This is mostly intended to handle binary data, for which the length cannot be calculated.
```
void echttp_content_queue (const char *data, int length);
```
Queue more content to be transmitted later. This function must be called within an HTTP callback. This allows the application to handle large data in chunks of known size when the total size of the data is not known beforehand. The data queued will be sent _after_ the data directly returned by the HTTP callback, but before any transfer data (see `echttp_transfer()` below). The data's buffer must have been allocated from the heap (i.e. using `malloc()`, `strdup()` and friends) and will be free'd once the data has been transmitted or when the connection breaks (whichever happens first).
```
void echttp_error (int code, const char *message);
```
The HTTP response will return the specified error instead of OK.
```
void echttp_redirect (const char *url);
```
The HTTP response will return a redirect to the specified URL.
```
const char *echttp_reason (void);
```
This function returns the error message stored last for the current request (see echttp_error()). This is mostly intended to help the application log HTTP errors.
```
void echttp_transfer (int fd, int size);
```
Declare a file descriptor to use to transfer data with the remote.

This function works in one of two modes:

- If the current context is still waiting for more content data (see echttp_asynchronous_route()), then the data is to be transfered in, from the remote to the file descriptor provided (open in append mode). Once the transfer is complete, the normal application callback is called.

- If no more content data is expected, then the data is to be transfered out, from the file descriptor provided (open in read mode) to the remote. That data will be sent _after_ the content data returned, or queued, by the callback.

This function should be called from within an HTTP callback, while processing an HTTP request. Size defines how many bytes must be transferred.

The echttp_transfer() function uses sendfile(2) when transferring data out (to the remote), but uses normal file operations (read(2) and write(2)) when transferring data in, from the remote.

```
void echttp_islocal (void);
```
Return 1 if the HTTP client is on a local network, 0 otherwise. This is a crude protection mechanism that can be used to decide if private information should be concealed, or the command be rejected, with the assumption that local machine can be trusted. An application might also decide to not request user authentication if the client is on a local network. This function should be called from within an HTTP callback, while processing an HTTP request.
```
int echttp_port (int ip);
```
Return the web server's port number for IPv4 (ip=4) or IPv6 (ip=6). If the port number returned is 0, the web server is not listening on the specified address space. (At this time echttp always uses the same port number for IPv4 and IPv6.)
```
int echttp_dynamic_port (void);
```
Return true if the HTTP server uses a dynamic port, false otherwise.

Dynamic ports are typically used when multiple HTTP servers run on the same machine (e.g. micro services), but require using a discovery service (e.g. HousePortal).

Dynamic port mode is activated using the command line option -http-service=dynamic.
```
int echttp_connect (const char *host, const char *service);
```
Create a TCP connection to the specified target. The connection is created in non-blocking mode and might not have been established when the function returns. The socket is not registered for listening: the application will need to call echttp_listen() when echttp_connect() succeeds. This function is only a helper provided to applications that do not want to go through the lengthy sequence of calls needed to establish a TCP connection.
```
typedef void echttp_listener (int fd, int mode);
void echttp_listen (int fd, int mode, echttp_listener *listener, int premium);
void echttp_forget (int fd);
```
Listen to the specified file descriptor (mode=0: don't listen, mode=1: read only, mode=2: write only, mode=3: read & write). This may be called multiple times to listen to a list of I/O, or on the same I/O to change the mode, the listener and the premium option. Setting mode to 0 can be used to temporarily suspend listening to the I/O without removing it from the list.

When the specified file descriptor is ready, the listener is called with the mode corresponding to the event.

The premium option, if used, causes this file descriptor to be processed before any HTTP client and other listener. The premium option is meant for a high priority I/O.

The echttp_forget() function is used to remove the I/O from the list. It has no effect if the I/O was not in the list. This does not close the file descriptor.

```
void echttp_background (echttp_listener *listener);
```
Call the specified listener before waiting for I/O. This listener is called with fd 0 and mode 0; it must not block on I/O itself, but it is allowed to call echttp_listen(), changing the list of I/O to listen to on the next cycle. This listener will be called once a second at most.

There is only one background listener.
```
void echttp_loop (void);
```
Run the HTTP server. This function typically never return. If it does, all HTTP server resources have been closed and deallocated. The most reasonable thing to do at this point is to exit.
```
void echttp_close (void);
```
Immediately close the HTTP server and all current HTTP connections.

## Static Pages Extension

The echttp library can serve local files, from several separate locations if needed. This capacity is a separate extension and requires to include echttp_static.h.
The static page extension primitives are:
```
void echttp_static_default (const char *arg);
```
Set a default value for a command line argument accepted by the static pages extension. This default value takes the form of a command line option (typically --name=value). The value will be overriden if the option is present in the command line (see echttp_static_initialize() below).
```
void echttp_static_initialize (int argc, const char *argv[]);
```
Initialize the static pages extensions according to the command line options. This extension accepts the following options:

* --http-root=PATH: Define a route for the root URI ("/") using PATH.

The echttp_static_default() and echttp_static_initialize() functions are optional. If not used, the application will have to declare the root URI explicitly. Their purpose is to make it easy to declare an alternate root path in the command line without much code logic in the application.
```
void echttp_static_content_map (const char *extension, const char *content);
```
Define the content type associated with a specific file extension. The content type is implicitely defined for the following file extensions: html, htm, json, jsn, css.
```
int  echttp_static_route (const char *uri, const char *path);
```
Associate a parent URI with a local directory path: a child of the specified URI will match an existing file at the specified path (including the URI's child relative path).

For example if one defines a static route from /static to /home/doe/public, the URI /static/fancy/interface.html will route to /home/doe/public/fancy/interface.html.

As soon as a static route has been declared, the extension takes over the root URI "/". If the root URI is requested, the extension seaches for file index.html in every path provided and returns the content of the first one found.

This function returns the route descriptor or -1 on failure. This route descriptor can be used to protect the whole path.

It is allowed to declare the same URI route multiple times. Only the last path provided will be used (previous paths are discarded). This allows applications
to change their URI mapping while running.

## Cross-Origin Resource Sharing (CORS) Extension

The CORS extension facilitate the support for the CORS HTTP mechanism. See web public documentation for more details about this mechanism.

This extension provides a minimal implementation of CORS:

```
void echttp_cors_allow_method (const char *method);
```
This function defines which method or methods are allowed in a CORS request. A method is allowed for any origin URL. This function can be called multiple times if more than one method is allowed.

```
void echttp_cors_trust_origin (const char *url);
```
This function defines an URL that is trusted, i.e. any cross access is allowed, regardless of the method. This does not create any restriction on origins for the allowed methods. This function can be called multiple times if more than one origin is trusted. The local host is implicitely trusted.

```
int echttp_cors_protect (const char *method, const char *uri);
```
This function must be called from a protect callback. It returns 0 if the request processing may continue or 1 if the protect callback should return immediately.

In the later case, this function has setup the HTTP status and headers necessary: there is nothing more that the caller can or should do.

## Command Line Options

The echttp library comes with a handful of functions to help decode command line arguments.
```
const char *echttp_option_match (const char *reference,
                                 const char *input, const char **value);
```
Returns 0 if the argument input does not match the reference string, a pointer to the value otherwise. (If there is no value, the result points to an end of string character).

This function supports the syntax option '=' value, if the reference string ends with a '='. In this case, and if there is a match, value points to the string after the '='. Otherwise value is not touched, so that the caller can initialize it with a default value.
```
int echttp_option_present (const char *reference, const char *input);
```
Returns 1 if there is an exact match, 0 otherwise.
```
int echttp_isdebug (void);
```
Return true if the HTTP debug option was selected. Only used for debug or troubleshooting.

## HTTP Client

The echttp library includes support for the HTTP and HTTPS client side. This is an extension to the HTTP server function, not an independent client library: the HTTP server needs to be initialized before the client functions can be used.

The intent is to allow an application using echttp to access other web servers to collect information, or to access web services. For example a sprinkler controller may need to access public web sites to get weather information, or control relays through local web services.

A web client request is processed in three steps:

* Initialize the query context (providing the method and URL).
* Set optional attributes if needed, using the standard echttp API (i.e. the same function as used to prepare a server response).
* Send the request to the server.

Note that the client functions support the echttp file transfer mechanism, i.e. it is possible to post a huge file without any size limit using echttp_transfer().

The server response will be processed asynchronously, through a client callback.

Only one client request can be prepared at a time: the current request must be sent before a subsequent request can be prepared. However there is no need to wait for the response: multiple client request may be pending at the same time and the order of the responses does not matter.

A client request can be initiated from within an HTTP request callback (i.e. while the echttp server is processing an HTTP request from a remote client), from within a response callback (i.e. while processing the response from a previous request), from within a listener (see echttp_listen()) or from within a background function (see echttp_background()). However if a client request is initiated while processing a request or response, that request or response is no longer the current context: all the information from that request or response must have been retrieved prior to initiating any new request.
````
const char *echttp_client (const char *method, const char *url);
````
Initiate a client context. This function returns 0 (null pointer) on success, or an error text on failure. There is no specific assumption made regarding the method string: any name compatible with the HTTP syntax would do, as long as the server supports it. The function will automatically use a TLS connection is the URL starts with "https:".

```
void echttp_asynchronous (echttp_response *asynchronous);
```
Declare this client request as asynchronous. This function can only be called just before calling echttp_submit().

The asynchronous response callback is different from the normal response callback in that it is called only if not all content data has been received yet. The provided data is the portion of the content that _was_ received. The code should compare the value of Content-Length with the length argument to retrieve how much content is still expected. The function _must_ save all received content and call echttp_transfer() to start the transfer of the remaining content.

If the asynchronous response callback function does not call echttp_transfer(), the pending request reverts to synchronous mode.

```
typedef void echttp_response (void *origin, int status, char *data, int length);

void echttp_submit (const char *data, int length,
                    echttp_response *response, void *origin);
```
Send the HTTP request, including the specified content. If a file transfer was set up, the file data is sent in addition to, and after, the specified data. The data may be empty (length 0), in which case the data pointer can be null.

Once a complete response has been received from the server, the response function is called with the provided origin value, the HTTP status from the server and the data content from the response, if any. The attributes from the response header can be accessed using the standard echttp API.

The echttp client functions do not automatically follow redirection (HTTP status 301, 302, 303, 307 or 308): the application must do this itself if this is the intent. The echttp API provide a helper function to make it easier:
```
int echttp_redirected (const char *method);
```
This function handles the common cases of redirections. If used, this must be called from the application's response callback, and it must be the first thing that the response callback do. How the response callback then behaves depends on the return code:

* Non-zero: the response was not handled and the response must be processed as normal. The value returned is an updated HTTP status.
* 0: the response was fully handled and the response callback must now (re)build the request data, call echttp_submit and then return without processing the response further.

The method parameter is the method used for the original request. Depending on the redirect code, either this method will be used or else the GET method will be forced.

It is always possible for the application to handle redirections on its own, if the case is too complex for this helper. Do not forget, however, to handle all the redirection codes (301, 302, 303, 207 and 308).

For example:
```
static void response (void *origin, int status, char *data, int length) {
    status = echttp_redirected ("GET");
    if (!status) {
        echttp_submit (0, 0, response, origin);
        return;
    }
    ... application specific code ...
}
```

```
void echttp_escape (const char *s, char *d, int size);
```
This support function encodes the s string according to the HTTP character encoding rules and stores the result to the d location. The result is no larger than size characters, including the null terminator.

## JSON and XML Support

The echttp library provides functions to handle JSON and XML data: a small JSON parser, a small XML parser and a JSON generator. These are built using the same approach as echttp itself: make the API simple to use. This parser support is a separate extension and requires to include echttp_json.h or echttp_xml.h (depending on the format used).

These parsers support UTF-8 only.

```
int echttp_json_estimate (const char *json);
const char *echttp_json_parse (char *json, ParserToken *token, int *count);
```
Parses the provided JSON string and populate the array of token. The content of the string is modified during the parsing. The variable pointed by count must contain the size of the token array before the call, and is set to the actual number of JSON items found by the parser (even on error). The parser return a null pointer on success, or an error message on failure. The error message container is a static buffer and it thus overwritten on the next call.

The token array must be large enough to hold all the tokens found, or else an error is returned. The `echttp_json_estimate()` function calculates an estimated size for the token array that should be sufficient. if the same token array is used multiple times, it is recommended to allocate an initial size large enough for most common cases and rely on the estimate only to protect against larger data sets. This avoids trashing the heap with frequent re-allocations.
```
int echttp_xml_estimate ((const char *xml);
const char *echttp_xml_parse (char *xml, ParserToken *token, int *count);
```
Parses the provided XML string and populate the array of token. The content of the string is modified during the parsing. The variable pointed by count must contain the size of the token array before the call, and is set to the actual number of JSON items found by the parser (even on error). The parser return a null pointer on success, or an error message on failure. The error message container is a static buffer and it thus overwritten on the next call.

The `echttp_xml_estimate()` function is the XML equivalent of the `echttp_json_estimate()` function.

The ParserToken type is defined as follows:
```
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
```
The possible types are:

* PARSER_NULL
* PARSER_BOOL
* PARSER_INTEGER
* PARSER_REAL
* PARSER_STRING
* PARSER_ARRAY
* PARSER_OBJECT

Arrays and objects have no value, but the length items indicates how many element are contained in the subsequent entries. (Only the elements immediately inside the array or object are counted: elements contained in a sub-array or sub-objects are not counted in length.) The length for all other types is always 0.

The key pointer is null for anynymous items.

This token structure closely matches the JSON syntax. When decoding XML, the following rules apply:

* The only token types generated are PARSER_OBJECT and PARSER_STRING.
* An anonymous top level object is always created.
* Every XML tag is type PARSER_OBJECT.
* If XML attributes are present in a tag, the tag object contains an element named "attributes" of type PARSER_OBJECT; this sub-object contains all the attributes as PARSER_STRING elements.
* If a content is present and is not made of XML tags, the content is stored in a PARSER_STRING element named "content".
* If a content is present and is made of XML tags, the content is stored as a PARSER_OBJECT element named "content"; This sub-object contains all the inner XML tags.

```
char *echttp_parser_load (const char *file);
char *echttp_parser_string (const char *text);
void  echttp_parser_free (char *buffer);
```
These functions are used to load the entire content of a JSON or XML file in a buffer, before parsing the data. The buffer is dynamically allocated, and must be released once the data is no longer used. Because the parsed tokens rely on the buffer's content, the buffer should be released only after the tokens have been discarded.

```
int echttp_json_search (const ParserToken *parent, const char *path);
```
Search a specific JSON item starting at the specified token. This function uses the array of token generated by echttp_json_parse() or echttp_xml_parse(). The search can start from the first token or from any inner object or array. This returns the index of the element identified by the path, or -1 on failure. The index returned is relative to the parent token provided. The path uses the JavaScript syntax for referencing object and array items. If the parent is an anonymous object, the path must start with a '.'; if the parent is an anonymous array, the path must start with an array index.

Examples of valid paths (matching the content of test/test.json):
```
    .rawobject.real
    .rawarray[6].item2
    .rawobject.object
    .formattedobject.array[1][0]
```
Its is valid to provide a path that ends on an object or array: this allows chaining searches (search an inner object first, then later search starting from that inner object).

When the list of token was generated from XML, it is possible to index the name of an object's element. This is used to access the Nth element (i.e. XML tag) with that name. This is done so because repeating the same tag name more than once is valid in XML.
```
const char *echttp_json_enumerate (const ParserToken *parent, int *index);
```
This function populates the list of children items to a parent array or object. It can be used when the application needs to walk an unknown JSON structure. The index array must be large enough for the expected number of parent's element, as indicated by parent->length. The function always returns exactly parent->length items on success. The index values are offset relative to the parent's record. Return null on success, or an error string on failure.

```
const char *echttp_json_format (ParserToken *token, int count,
                                char *json, int size, int options);
```
Format a JSON text given an array of JSON tokens. The array is the same that would have been generated by the JSON parser. In fact, this JSON format function is the inverse of the parser: the format function can consume the output of the parser, while the parser can consume the output of the format function. The JSON text is written to the json buffer (up to size characters). If options is 0, the JSON text is the most terse possible (no formatting); if options is JSON_OPTION_PRETTY, the JSON text is formatted for readability. The function returns null on success, or an error text on failure.

```
ParserContext echttp_json_start
                  (ParserToken *token, int max, char *pool, int size);
void echttp_json_add_null
         (ParserContext context, int parent, const char *key);
void echttp_json_add_bool
         (ParserContext context, int parent, const char *key, int value);
void echttp_json_add_integer
         (ParserContext context, int parent, const char *key, long value);
void echttp_json_add_real
         (ParserContext context, int parent, const char *key, double value);
void echttp_json_add_string
         (ParserContext context, int parent, const char *key, const char *value);
int echttp_json_add_object
         (ParserContext context, int parent, const char *key);
int echttp_json_add_array
         (ParserContext context, int parent, const char *key);
int echttp_json_end (ParserContext context);

const char *echttp_json_export (ParserContext context, char *buffer, int size);
```
This group of functions makes it easy to create JSON data directly from the application. The typical usage is in an HTTP request handler, to build a JSON response to the client's request for live data. The echttp_json_start() function allocates a JSON context used in all subsequent calls. The application provides the JSON token array and a memory pool for storing strings (JSON keys and values). The echttp_json_add_XXX() functions add one more JSON token to the data structure. The parent argument is used for generating nested structures: a valid parent argument is the return value from a previous call to echttp_json_add_object() or echttp_json_add_array(). The order of the calls must be consistent with the JSON data structure as it defines the order in which the JSON entities will be generated: an object or array must be completely defined before the next non-nested object or array is created. The key argument must be present when the parent is an object, and will be ignored when the parent is an array. The echttp_json_end() function deletes the context and return the count of tokens that have been created. The token array can then be used for create JSON text using echttp_json_format(). Function echttp_json_export() combines echttp_json_end() and echttp_json_format() in a single call (with pretty printing disabled).

There are also two JSON utilities provided with echttp:

* echttp_print reformats the content of the JSON or XML files provided and prints it to standard output as JSON. By default the output is pretty-printed; if the -r option is used, the output is generated as a single line with no space between JSON elements.
* echttp_get loads the JSON or XML data from the file name (first argument) and prints the value associated with each path provided (subsequent arguments). Note that the JavaScript array syntax used for the path requires quoting in shell.

Both tools have minimal features. They were intended to test the JSON and XML functions, but can be useful to analyze the content of a JSON file, especially when the JSON data was not formatted for readability.

## Catalog (a minimalist associative array module)

The echttp library uses its own associative array mechanism for managing HTTP attributes or routes. This module is made public because it is somewhat useful for the echttp applications as well, for example when maintaining a catalog of known web services.

The basis for the mechanism is the echttp_catalog type:
```
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
```
A catalog must be initialized. A static catalog is naturally initialized by the compiler, but a local catalog must be initialized explicitely:
```
void echttp_catalog_reset (echttp_catalog *d);
```

There are two ways to add (or update) entries in a catalog:

* The simplest method works if the value was not allocated, or is referenced in another place:
```
void echttp_catalog_set (echttp_catalog *d,
                         const char *name, const char *value);
```

* The more generic method returns the old (and discarded) value, if any, which allows it do be deallocated cleanly if necessary:
```
const char *echttp_catalog_refresh
               (echttp_catalog *d, const char *name, const char *value, time_t timestamp);
```
In both case the value of an existing entry is replaced with the new value, or a new entry is added if non match the name. The timestamp parameter indicates the age of the entry, which can be useful when using the catalog for discovery, i.e. when expired entries should be ignored.

A catalog entry can be retrieved in two ways:

* Find the entry, for example when both the value and timestamp must be accessed:
```
int echttp_catalog_find (echttp_catalog *d, const char *name);
```

* Get the value:
```
const char *echttp_catalog_get (echttp_catalog *d, const char *name);
```

A few more functions are used in more rare cases:
```
void echttp_catalog_join (echttp_catalog *d,
                          const char *sep, char *text, int size);
```
This function dumps all its content in the HTTP parameter format.
```
typedef int echttp_catalog_action (const char *name, const char *value);
void echttp_catalog_enumerate (echttp_catalog *d,
                               echttp_catalog_action *action);
```
Call the specified action for each item present in the catalog.

```
unsigned int echttp_catalog_signature (const char *name);
```
This function computes a signature from the provided name. A signature is the hash value before applying the hash array modulo. This function can be reused when implementing a hash table module with different properties. This function is derived from the well known hash function by Daniel J. Bernstein.

## HTTP Character Encoding

The echttp library comes with a module for encoding and decoding strings per the HTTP encoding rules:
```
char *echttp_encoding_escape (const char *s, char *d, int size);
```
This function encodes the s string per the HTTP encoding rules and store the result to d. The result is never longer than the specified size. A pointer to the result is returned, so that this function can be used in a parameter list.
```
char *echttp_encoding_unescape (char *data);
```
This function decodes the escape sequences as per the HTTP encoding rules. The decoding happens in-place and the original (encoded) data is lost. A pointer to the result is returned, so that this function can be used in a parameter list.

## Sorted List

The echttp library comes with a module for creating and maintaining sorted
lists. The order is maintained while adding and removing items. This module
is intended for chronologically ordered lists:

- The sort key is a 64 bit integer (unsigned),
- The implementation is optimized for key values that are clumped together,
  like timestamps from the last few weeks or months.
- The sort key is not assumed to be unique, but the same item cannot be
  present more than once. In other terms, the reference to the data must
  be unique.

Adding or removing an item has a fixed maximum cost (allocating or releasing
up to 7 buckets and allocating or releasing up to 2 collision list items).
Iterating through a list has a (mostly) linear cost.

The list only contains opaque references to the items, and does not access
the items data. It is the application's responsibility to handle the data
storage as it sees fit.

An empty list must be created first:
```
echttp_sorted_list echttp_sorted_new (void);
```

Then items can be added or removed individually:
```
void echttp_sorted_add (echttp_sorted_list b,
                        unsigned long long key, void *data);
void echttp_sorted_remove (echttp_sorted_list b,
                           unsigned long long key, void *data);
```

A list can then be iterated at any time, either in ascending or descending
order. The iterator stops when the action returns 0, or else at the end of
the list. The value returned is 1 when the end of list was reached, or
0 otherwise:
```
typedef int echttp_sorted_action (void *data);

int echttp_sorted_descending (echttp_sorted_list b,
                              echttp_sorted_action *action);
int echttp_sorted_ascending (echttp_sorted_list b,
                             echttp_sorted_action *action);

int echttp_sorted_descending_from (echttp_sorted_list b,
                                   unsigned long long key,
                                   echttp_sorted_action *action);
int echttp_sorted_ascending_from (echttp_sorted_list b,
                                  unsigned long long key,
                                  echttp_sorted_action *action);
```
The later two functions iterate only on items for which the key is less or
equal (or greater or equal) than the provided key value. This can be used
to avoid the overhead of iterating repeatedly over the same items again and
again when specific key ranges are of interest.

## Debian Packaging

The provided Makefile supports building private Debian packages. These are _not_ official packages:

- They do not follow all Debian policies.

- They are not built using Debian standard conventions and tools.

- The packaging is not separate from the upstream sources, and there is no source package.

To build a Debian package, use the `debian-package` target:
```
make debian-package
```

