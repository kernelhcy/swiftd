#!/bin/bash

gcc main.c

exec ./a.out 9999999 &
exec ./a.out 9999999 &
exec ./a.out 9999999 &

rm ./a.out
