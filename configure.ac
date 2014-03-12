AC_INIT([freeprocessing], [0.9])
AC_PREREQ([2.68])
AC_CONFIG_SRCDIR([simplesitu.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign])

LT_INIT([disable-static dlopen pic-only])
AC_PROG_LIBTOOL
AC_LIBTOOL_DLOPEN
AC_PROG_CC_C99

AC_DISABLE_STATIC
AC_HEADER_STDBOOL
dnl AX_CFLAGS_WARN_ALL

AX_MPI

dnl provide '--enable-python' option, but disable it by default.
AC_ARG_ENABLE([python],
  [AS_HELP_STRING(
    [--enable-python],
    [build python-enabled freeprocessors @<:@default=no@:>@]
   )],
  [], [enable_python=no]
)
AS_IF([test "x${enable_python}" != "xno"],
  [AX_PYTHON_DEVEL]
)
AM_CONDITIONAL([PYTHON], [test "x${enable_python}" != "xno"])

AC_CONFIG_FILES([Makefile])

dnl Why do I have to do this?!  libtool breaks horrifically without this, since
dnl it tries to add an rpath of ${LIBdir} and the variable isn't set.
LIBdir=$(readlink -f ${objdir})

AC_SUBST([LIBDIR])
AC_SUBST([LIBdir])
AC_OUTPUT
