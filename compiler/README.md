This is a compiler for a language I made up. At the moment, the tests and the
compiler are the spec, but I hope to improve that at some point.

# Usage

Given an input file, run:

~~~sh
python3 emitter.py <infile>
~~~

There are additional flags you can see by running:

~~~sh
python3 emitter.py --help
~~~

# Under the hood

This compiler uses a hand-written recursive descent parser. The parse tree is
the converted to a series of opcodes by a very simple compiler. The opcodes,
along with the associated metadata of the bytecode format, is then emitted.

The compiler uses a very simple register allocator (simply allocating a new
register any time one is needed). This is really wasteful, and at some point I'd
like to add a better register allocator, or perhaps register renaming at the
interpreter level, though that's much lower priority than other optimizations.

The compiler is written in Python because I'm mostly in this project for the
experience writing an interpreter, so I wanted to spend as little time as
possible writing the compiler.
