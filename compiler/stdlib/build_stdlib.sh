#!/bin/bash

DIRNAME=$(dirname "$0")

python3 $DIRNAME/../emitter.py --no-stdlib -j $DIRNAME/src/stdlib.internal.json $DIRNAME/src/stdlib.slg -o $DIRNAME/bin/stdlib.slb
