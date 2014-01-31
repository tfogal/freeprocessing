This is a data transfer symbiont.  It is essentially 'tee' in library form.

To use it, `LD_PRELOAD` the library (`libsitu.so`) into your program.  At that
point, it looks for all of your `write` calls and does its magic.

Configuration
=============

The library looks for a library named `situ.cfg`.  This library defines
what libraries should be loaded.  The general format is basically like
awk, except that instead of specifying the code to run, you specify a
library that implements that code:

  pattern { exec: /path/to/library.so }

Where `pattern` is a shell glob and `library.so` is a library
implementing a pre-prescribed interface (see `LIBRARIES`).  An example:

```
  *header* { exec: ./libmine.so }
  *.data { exec: ./libmine.so }
  *tosend* { exec: ./libinstr.so }
```

This defines three processing elements, `libmine` will be used whenever
a filename which matches `*header*` or `*.data` is accessed.  *Both*
instances get called.  The order they are called is undefined.

Sharing internal memory
-----------------------

If a single library is given in multiple patterns, the matches *share*
private variable instances.  In the above example, both `libmine.so`
instances will access a single set of shared variables.  This allows
you to (for instance) parse the program's header output and communicate
such information to the part of the code which instruments the binary
outputs.

If you actually want two instances of a library, with unique internal
variables each time, just rename one of them.  The equivalency is
based purely on the name of the libraries, so libraries of different
names---even if they are the exact sample implementation---will not
share private data.

Libraries
=========

Libraries need only export two functions:

  1. `void exec(const char* fn, const void* buf, size_t n)`, and
  2. `void finish(const char* fn)`

The first is called anytime a file is written to.  The second is called
whenever a file is closed.  The filename given in the first argument
is the exact filename (not, for example, the pattern given in the
configuration file).

The symbiont attempts to take care of the issue of partial writes;
there should be a 1-1 mapping between program `write`s and `exec`
invocations.

Environment
===========

  * `LIBSITU_DEBUG`: used to selectively enable debug information.  See
  below for more info.

Forking
-------

There can be issues forking processes.  This includes using `system`.
Therefore, the library unsets `LD_PRELOAD` during initialization, so
that it will not instrument any child processes.

For the most part, this is what you want.  However, this may cause
problems if you have a complicated loading procedure ("mpirun" and
"valgrind" come to mind).  To get the code to run under MPI, you want
the `mpirun` program to set the environment variable for you, instead
of setting it before `mpirun` is invoked.  OpenMPI's `mpirun`s let you
do this with `-x`, e.g.:

  mpirun -x LD_PRELOAD=./libsitu.so -np 4 ./myprogram

For MPICH, this option is `-env` and the setting should lack the `=`
character:

  mpiexec -env LD_PRELOAD ./libsitu.so -np 4 ./myprogram

Debug Channel Settings
======================

The symbiont has a set of unique debug channels.  Generally these are
all related to a specific feature, such as investigating when files are
opened, or when data is transferred, or other information.

In addition, each channel has a set of *class*es, which might loosely
be thought of as severity.  So you can, for example, turn on only the
warnings from a particular channel, or only the errors.

The way one configures these channels is through the
`LIBSITU_DEBUG` environment variable.  The general form is `channel
name=[+-]class[,...]`, e.g.:

  opens=+warn

which says to enable warnings on the channel `opens`.  Multiple class
specifications are separated by commas:

  opens=+trace,-warn

Multiple channels are delimited by semicolons:

  opens=+trace,-warn;writes=+trace

The currently defined channels are:

  * `opens`: all `open`-esque calls, along with their `close`
  counterparts.
  * `writes`: all `write`-esque calls.

The currently defined classes are:

  * `err`: errors which almost certainly indicate a bug in either the
  symbiont or the host program.
  * `warn`: generally anomalous conditions which might not be a
  problem, such as an internal table filling up.
  * `trace`: notification *when* a call occurs, in a manner similar to
  `strace(1)` or `ltrace(1)`.

Notes
=====

It seems that the executable needs to be built as position-independent
code for this to work.

if the code is not position-independent, then
`__attribute__((constructor))` functions do not get run and thus our
function pointers never get initialized.

Fortran may work a bit differently, doesn't seem required there.
