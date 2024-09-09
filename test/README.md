## Overview

This directory contains the tools and data used to test echttp.

## Install

* Build and install the echttp library (see top folder).
* Compile httpserver:
```
cc -Og -o httpserver httpserver.c -lechttp -lssl -lcrypto
```
* Run httpserver:
```
./httpserver -http-service=8080
```
* If you want to see the debug traces:
```
./httpserver -http-service=8080 -http-debug
```
* Run the test for echttp_sorted.c:
```
gcc -g -I.. -Og -o sortedtest sortedtest.c ../echttp_sorted.c
./sortedtest
```

