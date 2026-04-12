## Overview

This directory contains the tools and data used to test echttp.

## Install

* Build and install the echttp library (see top folder).
* Compile httpserver:
```
make httpserver
```
* Run httpserver:
```
./httpserver -http-service=8080
```
* If you want to see the debug traces:
```
./httpserver -http-service=8080 -http-debug
```
* Build and run the test for echttp_sorted.c and echttp_libc.c:
```
make test
```

