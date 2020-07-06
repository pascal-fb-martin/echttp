# Overview
A minimal HTTP server to serve as an application console for configuration and monitoring.

This is a small HTTP/1.1 server designed for simple API and minimal dependencies. The target use case is an application that needs to be administrated or monitored remotely.

There is no security mechanism planned at this time: use this library only for applications accessible from a protected network.
(In the long term, it will eventually interface with a TLS library and manage accounts, but don't hold your breath..)

After initializing the HTTP server, and then completing its own initialization, the application reacts to file descriptor events and HTTP requests as provided by the HTTP server.

This web server can be configured to use dynamic port allocation for its server socket. Before you hit your head against the proverbial wall trying to understand why the hell this is even allowed, please take a look at [houseportal](https://github.com/pascal-fb-martin/houseportal).

# Installation
* Clone the repository.
* cd echttp
* make
* sudo make install (this installs files in the /usr/local tree)

Use the -lechttp option when building your application. For example:
```
cc -o httpserver httpserver.c -lechttp
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
```
int echttp_route_match (const char *root, echttp_callback *call);
```
Defines a route for a parent URI and all its children. Return the route descriptor or -1 on failure.
```
typedef void echttp_protect_callback (const char *method, const char *uri);
int echttp_protect (int route, echttp_protect_callback *call);
```
Declare a protect callback for the specified route. The route descriptor is the value returned by echttp_route_uri() or echttp_route_match(). A protect callback is called before the route's callback and may change the HTTP status to not OK (by calling echttp_error()--see later) and set the response's attributes; in that case the route's callback function is not called.

Note that the protect callback is associated with the route, i.e. the URI string, not with the callback function: one may define two routes to the same callback function, and protect one route but not the other. This is intentional, as this allows protecting specific file paths without protecting all file paths (see the static page extention later).

This mechanism is meant to facilitate the implementation of access control extensions, this is not an access control method on its own.
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
void echttp_error (int code, const char *message);
```
The HTTP response will return the specified error instead of OK.
```
void echttp_redirect (const char *url);
```
The HTTP response will return a redirect to the specified URL.
```
void echttp_transfer (int fd, int size);
```
Declare a file descriptor to transfer after the returned response. This function should be called from within an HTTP callback, while processing an HTTP request. Size defines how many bytes must be transferred from the file to the client. This transfer only happens after the HTTP preamble and the response string returned by the callback have been sent.
```
void echttp_islocal (void);
```
Return 1 if the HTTP client is on a local network, 0 otherwise. This is a crude protection mechanism that can be used to decide if private information should be concealed, or the command be rejected, with the assumption that local machine can be trusted. An application might also decide to not request user authentication if the client is on a local network. This function should be called from within an HTTP callback, while processing an HTTP request.
```
int echttp_port (int ip);
```
Return the web server's port number for IPv4 (ip=4) or IPv6 (ip=6). If the port number returned is 0, the web server is not listening on the specified address space. (IPv6 is not currently supported by echttp, and the port number returned for IPv6 is always 0 at this time.)
```
int echttp_dynamic_port (void);
```
Return true if the HTTP server uses a dynamic port, false otherwise.

Dynamic ports are typically used when multiple HTTP servers run on the same machine (e.g. micro services), but require using a discovery service (e.g. HousePortal).

Dynamic port mode is activated using the command line option -http-service=dynamic.
```
typedef void *echttp_listener (int fd, int mode);
void echttp_listen (int fd, int mode, echttp_listener *listener, int premium);
```
Listen to the specified file descriptor (mode=0: don't listen, mode=1: read only, mode=2: write only, mode=3: read & write).

When the specified file descriptor is ready, the listener is called with the mode corresponding to the event.

The premium option, if used, causes this file descriptor to be processed before any HTTP client and other listener. The premium option is meant for a high priority I/O.
```
void echttp_background (echttp_listener *listener);
```
Call the specified listener before waiting for I/O. This listener is called with fd 0 and mode 0; it must not block on I/O itself, but it is allowed to call echttp_listen(), changing the list of I/O to listen to on the next cycle. Expect this listener to be called once a second at least, if not a lot more.

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

The echttp library includes support for the HTTP client side. This is an extension to the HTTP server function, not an independent client library: the HTTP server needs to be initialized before the client functions can be used.

The intent is to allow an application using echttp to access other web server to collect information, or to access web services. For example a sprinkler controller may need to access public web sites to get weather information, or control relays through local web services.

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
Initiate a client context. This function returns 0 (null pointer) on success, or an error text on failure. There is no specific assumption made regarding the method string: any name compatible with the HTTP syntax would do, as long as the server supports it.
```
typedef void echttp_response (void *origin, int status, char *data, int length);

void echttp_submit (const char *data, int length,
                    echttp_response *response, void *origin);
```
Send the HTTP request, including the specified content. If a file transfer was set up, the file data is sent in addition to, and after, the specified data. The data may be empty (length 0), in which case the data pointer can be null.

Once a complete response has been received from the server, the response function is called with the provided origin value, the HTTP status from the server and the data content from the response, if any. The attributes from the response header can be accessed using the standard echttp API.

The echttp client functions do not automatically follow redirection (HTTP status 302): the application must do this itself if this is the intent. For example:
```
static void response (void *origin, int status, char *data, int length) {
    if (status == 302) {
        const char *error =
            echttp_client ("GET", echttp_attribute_get("Location"));
        if (!error)
            echttp_submit (0, 0, response, 0);
        return;
    }
    ... application specific code ...
}
```
## JSON Parser and Generator

The echttp library provides functions to handle JSON data: a small JSON parser and a JSON generator. These are built using the same approach as echttp itself: make the API simple to use. This JSON support is a separate extension and requires to include echttp_json.h.

Any JSON data can be decoded using three functions:
```
const char *echttp_json_parse (char *json, JsonToken *token, int *count);
```
Parses the provided JSON string and populate the array of token. The content of the string is modified during the parsing. The variable pointed by count must contain the size of the token array before the call, and is set to the actual number of JSON items found by the parser. The parser return a null pointer on success, or an error message on failure. The error message container is a static buffer and it thus overwritten on the next call.

The JsonToken type is defined as follows:
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
} JsonToken;
```
The possible types are:
* JSON_NULL
* JSON_BOOL
* JSON_INTEGER
* JSON_REAL
* JSON_STRING
* JSON_ARRAY
* JSON_OBJECT

Arrays and objects have no value, but the length items indicates how many element are contained in the subsequent entries. (Only the elements immediately inside the array or object are counted: elements contained in a sub-array or sub-objects are not counted in length.) The length for all other types is always 0.

The key pointer is null for anynymous items.
```
int echttp_json_search (const JsonToken *parent, const char *path);
```
Search a specific JSON item starting at the specified token. This function uses the array of token generated by echttp_json_parse(). The search can start from the first token or from any inner object or array. This returns the index of the element identified by the path, or -1 on failure. The index returned is relative to the parent token provided. The path uses the JavaScript syntax for referencing object and array items. If the parent is an anonymous object, the path must start with a '.'; if the parent is an anonymous array, the path must start with an array index.

Examples of valid paths (matching the content of test/test.json):
```
    .rawobject.real
    .rawarray[6].item2
    .rawobject.object
    .formattedobject.array[1][0]
```
Its is valid to provide a path that ends on an object or array: this allows chaining the searches (search an inner object first, then later search starting from that inner object).
```
const char *echttp_json_enumerate (const JsonToken *parent, int *index);
```
This function populates the list of children items to a parent array or object. It can be used when the application needs to walk an unknown JSON structure. The index array must be large enough for the expected number of parent's element, as indicated by parent->length. The function always returns exactly parent->length items on success. The index values are offset relative to the parent's record. Return null on success, or an error string on failure.

```
const char *echttp_json_format (JsonToken *token, int count,
                                char *json, int size, int options);
```
Format a JSON text give the array of JSON token. The JSON token array is the same that would have been generated by the JSON parser. In fact, this JSON generator is the inverse function of the parser: the generator can consume the output of the parser, while the parser can consume the output of the generator.

```
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

const char *echttp_json_export (JsonContext context, char *buffer, int size);
```
This group of functions makes it easy to create JSON data directly from the application. The typical usage is in an HTTP request handler, to build a JSON response to the client's request for live data. The echttp_json_start() function allocates a JSON context used in all subsequent calls. The application provides the JSON token array and a memory pool for storing strings (JSON keys and values). The echttp_json_add_*() functions add one more JSON token to the data structure. The parent argument is used for generating nested structures: a valid parent argument is the return value from a previous call to echttp_json_add_object() or echttp_json_add_array(). The order of the calls must be consistent with the JSON data structure as it defines the order in which the JSON entities will be generated: an object or array must be completely defined before the next non-nested object or array is created. The key argument must be present when the parent is an object, and will be ignored when the parent is an array. The echttp_json_end() function deletes the context and return the count of tokens that have been created. The token array can then be used for create JSON text using echttp_json_format(). Function echttp_json_export() combines echttp_json_end() and echttp_json_format() in a single call (with pretty printing disabled).

There are also two JSON utilities provided with echttp:
* echttp_print reformats the content of the JSON files provided and prints it to standard output. By default the output is pretty-printed; if the -r option is used, the output is generated as a single line with no space between JSON elements.
* echttp_get loads the JSON data from the file name (first argument) and prints the value associated with each JSON path provided (subsequent arguments). Note that the JavaScript array syntax requires quoting in shell.

Both tools have minimal features. They were intended to test the JSON functions, but can be useful to analyze the content of a JSON file, especially when the JSON data was not formatted for readability.

