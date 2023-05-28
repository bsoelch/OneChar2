#!/bin/sh

cArgs=( "-g" "-Wall" "-Wextra" "-Wshadow" "-Wold-style-definition" "-Wcast-qual" "-Werror" "-pedantic" "-lm" )
gcc ${cArgs[@]} "./src/OneChar.c" -o ./oneChar
./oneChar "-f" "test.txt"
