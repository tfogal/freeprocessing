This is a data transfer symbiont.  It is essentially 'tee' in library form.

To use it, `LD_PRELOAD` the library (`libsitu.so`) into your program.  At that
point, it looks for all of your `write` calls and does its magic.

Environment
===========

  * `LIBSITU_DEBUG`: set it to any value to enable debug output.
  * `LIBSITU_LOGFILE`: set it to a filename and the library will put its
  debugging output to this file.
  * `LIBSITU_FILENAME`: set it equal to a shell-like pattern, and the
  library will only instrument files that match the pattern.

Notes
=====

It seems that the executable needs to be built as position-independent
code for this to work.

if the code is not position-independent, then
__attribute__((constructor)) functions do not get run and thus our
function pointers never get initialized.

Fortran may work a bit differently, doesn't seem required there.
