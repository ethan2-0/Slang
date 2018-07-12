This is an interpreter for the bytecode resulting from the compiler. This is
also my first major project written in C, so I apologize for any unclear code.
I also use a strange naming convention, which prefixes everything with a
two-letter code, followed by an underscore, as a form of namespacing.

# Usage

Invoke the interpreter with the location of a `.slb` bytecode file as the only
argument.

~~~sh
./interpreter input.slb
~~~

# Feature support

Currently, less than half of the opcodes are supported. In particular, control
flow and method calls aren't implemented. Return simply prints the value of the
register that's returned to standard output. This means all you can do is
assign variables and do arithmetic and return.

# Under the hood

This interpreter is, for now, written as simply as possible.

At the moment, dispatch is basically a giant `switch` statement. At some point,
I want to rewrite the dispatch to use [the GCC labels-pointer](https://gcc.gnu.org/onlinedocs/gcc/Labels-as-Values.html)
extension, or maybe even use a full-blown JIT.

# Building

This project is built with Meson. To setup:

~~~sh
mkdir builddir
meson builddir
~~~

To build:

~~~sh
cd builddir
ninja
~~~
