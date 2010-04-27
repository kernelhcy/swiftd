#!/bin/bash

gcc main.c -o test

exec ./test 10000 &
exec ./test 10000 &
exec ./test 10000 &
exec ./test 10000 &
exec ./test 10000 &

