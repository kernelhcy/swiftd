#!/bin/bash

gcc main.c -o test

exec ./test 9999999 &
exec ./test 9999999 &
exec ./test 9999999 &

rm ./test
