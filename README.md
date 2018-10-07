This is a programming language that I've created from start to finish to learn
about language design. This dates back since around February of this year, and
I've worked on it off-and-on ever since.

At the moment, the language supports methods, simple types, and simple classes
with methods but without inheritance. It's complete enough that I've
implemented a simple linked list class in it with no trouble.

The language, which I call Slang, requires really clever use of recursion to
prove turing completeness, owing to the lack of arrays at the moment, though I'm
convinced you could implement lambda calculus in the language, proving its
universality.

The bytecode is overall unremarkable. It encodes instructions for a simple
register-based virtual machine I created.

# Usage

First, compile the interpreter.

~~~sh
cd interpreter
meson builddir
cd builddir
ninja
cd ../..
~~~

First, compile a bytecode file.

~~~sh
cd compiler
python3 emitter.py input.slg
~~~

Then, copy the output into the directory of the interpreter (not strictly
necessary, but convenient).

~~~sh
mkdir ../interpreter/builddir
cp input.slb ../interpreter/builddir
~~~

Then, run the interpreter on the bytecode.

~~~sh
cd ../interpreter/builddir
./interpreter input.slb
~~~
