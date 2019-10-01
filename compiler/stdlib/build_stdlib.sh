#!/bin/bash

DIRNAME=$(dirname "$0")

mkdir -p $DIRNAME/bin

python3 $DIRNAME/../compiler.py --no-stdlib $DIRNAME/src/stdlib.slg -o $DIRNAME/bin/stdlib.slb
