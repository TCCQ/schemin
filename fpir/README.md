# FPIR
This is the 'forsp-alike' internal representation (in theory).

It is based on the excellent ideas here: https://xorvoid.com/forsp.html
It expands on them in a few ways, but also chooses different conventions.

I suggest you read that blog post first before exploring fpir, since
it is a better introduction to the core ideas.

## Quick Start
Start with `git submodule init` and `git submodule update`. Then you
are setup up.

Just type `make fpir` and don't be surprised when it takes a
second. Read on to find out why. Then launch with `./fpir` and it's a
RE(P)L. You will want to make use of `print` and `sstack` and `env
print` (two words).

Consider looking at `std.fp` for some basic definitions.

## Changes from forsp
The most notable is the changed syntactic sugar. I take the following
convention:

```
'a -> quote a
:a -> quote a pope
^a -> quote a pops
$a -> quote a push
```

These are respectively:
- Quoting a symbol
- Extending the current environment with the initial value popped from
  the stack
- Reassigning the value associated with the symbol in the current
  environment with a new value from the stack
- Push the value of the symbol in the current environment to the stack

## Additions
Garbage collection! This is the most significant addition, and has
proved to be fairly interesting. See gc.org for details.

## Important Ideas
### Captures
Having read the blog post, most of what is said there applies
here. Parens create thunked computations, capturing their
environment. When executing them several times (from some symbol they
are bound to, likely), all executions reference the same
environment. This means that symbols in scope when it was captured can
be mutated across calls to the thunk, but symbols added via `pope/:`
inside the thunk do not persist. This means you can do things like:

```
(0 :n ($n dup print 1 add ^n)) :mkcounter
mkcounter :c
c  {prints 0}
c  {prints 1}
mkcounter :b
b  {prints 0}
```

Here c is bound to something like `counter++`. You could imagine
removing the print to have `c` leave the value on the stack for
productive use elsewhere in your program.

### Lifetimes and Memory Use
As a user of fpir, you can (hopefully) rely on the garbage collector
to be sane. This means you do not need to think about cleaning up data
and can simply drop whatever you don't use, or have it indirectly
dropped through a dropped environment or the like.

As an implementer, things are complicated. Go read gc.org and the source.

The only thing to know as a user of fpir with regards to memory is
that once a symbol appears in your program, the memory to store the
string representation of the symbol itself persists forever. However,
only one copy of each distinct string is kept. This, in addition to
the lexical scoping, means that users are encouraged to make frequent
re-use of symbol names whenever appropriate. This is particularly
feasible and effective when re-using names for procedure arguments.

## Interacting With The World
The standard version (`make fpir`) runs on linux, takes input from
stdin, and writes to stdout and stderr.

The RISC-V versions runs on the qemu virt RISC-V machine, and
initially takes input and writes output to finite baked memory
locations. Intent for the future of this project is to massage the
base language to be a bit more flexible, and then write drivers for
uart on RISC-V before switching the main loop to use uart instead of
memory for I/O. Time will tell.

## Build Process
I link against the musl libc library instead of glibc because it is
fairly quick to build and easy to sandbox. The reason any of that
matters is to have more control when debugging the linux
version. Particularly, this lets me build libc without intel vector
support (used in memcpy in puts), which in turn lets me use gdb
reverse execution.

For the baremetal version, I build the riscv64 cross compiler from
musl for consistency and ease. Note that a cross-gdb is not built and
should be acquired in the appropriate way according to your
distribution.

## Debugging
If you are unfortunate enough to have to debug any part of this
project, First of all: my sincerest condolences. Second of all: in
order to make the process not simply staring at hex numbers, the
following tool exists. When the sanity preprocessor flag is enabled,
garbage collection produces a secondary file called `mdump.dot`. This
file is a DOT language file describing the full graph of valid cells
in the program. If you are not interested in debugging the garbage
collection process itself, then you are encouraged to filter the file
like so:

```
sed -e /red/d -e /green/d < mdump.dot > alive.dot
```

This removes the parts of the graph that track the garbage collection
process and greatly clean up the look of the graph.

Note that especially for large graphs, the `dot` layout engine can
take a while. I suggest sticking with `dot` and using `-Tpdf` since it
works the best with very large images.

An example from running `count` from `std.fp` can be found in the
examples directory.

