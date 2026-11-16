## Overview

This directory contains the tools and data used to test echttp.

## Install

* Build and install the echttp library (see top folder).

## HTTP Test Server

* Compile httpserver:

```
make httpserver
```

* Run httpserver:

```
./httpserver -http-service=8080
```

* Use the -http-debug option to see the debug traces:

```
./httpserver -http-service=8080 -http-debug
```

## HTTP Test Client

This tools exercise the client functions of the echttp library. It sends the requested URL (or URLs) once per second, with a 15 seconds pause every 4 seconds. The original purpose is to be a shakedown test of the echttp mechanism for reusing client connections.

* Compile httpclient:

```
make httpclient
```

* Run httpclient:

```
./httpclient URL ..
```

* Use the -v option to see the content of the responses:

```
./httpclient -v URL ..
```

### Unit Tests

* Build and run the test for echttp_sorted.c and echttp_libc.c:

```
make test
```

