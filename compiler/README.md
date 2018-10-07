This is a compiler for a language I made up. At the moment, the tests and the
compiler are the spec, but I hope to improve that at some point.

# Usage

Given an input file, run:

~~~sh
python3 emitter.py <infile>.slg
~~~

This will produce an output file named `<infile>.slb`.

There are additional flags you can see by running:

~~~sh
python3 emitter.py --help
~~~

Then, run the interpreter on the resulting bytecode file:

~~~sh
./path/to/interpreter path/to/slb/file
~~~

# Under the hood

This compiler uses a hand-written recursive descent parser. The parse tree is
the converted to a series of opcodes by a very simple compiler. The opcodes,
along with the associated metadata of the bytecode format, is then emitted.

The compiler uses a very simple register allocator (simply allocating a new
register any time one is needed). This is really wasteful, and at some point I'd
like to add a better register allocator, or perhaps register renaming at the
interpreter level, though that's much lower priority than other optimizations.

The implementation of the type system relies on expressions having types that
can be determined without any of the surrounding context. This makes it very
easy to simply decide the type of an expression and then compare it with the
expected type. At the moment, the implementation is quite slow (from a
time-complexity perspective), but that can be solved with dynamic programming.
