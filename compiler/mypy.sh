#!/bin/sh

# Only typesys.py is fully typechecked as of yet.
# TODO: Update this to include everything else.
mypy typesys.py emitter.py parser.py
