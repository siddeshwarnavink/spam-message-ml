#!/bin/bash

mkdir -p .build/

# cc -E -P -o .build/main_amalgamation.c src/main.c
# clang-format -i .build/main_amalgamation.c
# cc -o .build/main .build/main_amalgamation.c -lm -Wall -ggdb

cc -o .build/main src/main.c -lm -Wall -ggdb
