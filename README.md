This is a data transfer symbiont.  It is essentially 'tee' in library form.

To use it, `LD_PRELOAD` the library (`libsitu.so`) into your program.  At that
point, it looks for all of your `write` calls and does its magic.

Environment
===========

  * `LIBSITU_DEBUG`: used to selectively enable debug information.  See
  below for more info.
  * `LIBSITU_FILENAME`: set it equal to a shell-like pattern, and the
  library will only instrument files that match the pattern.

Forking
-------

There can be issues forking processes.  This includes using `system`.
Therefore, the library unsets `LD_PRELOAD` during initialization, so
that it will not instrument any child processes.

For the most part, this is what you want.  However, this may cause
problems if you have a complicated loading procedure (valgrind comes to
mind).

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
