dnl Process this file with autoconf to produce a configure script.
AC_INIT(PyIlmBase, 2.0.1)
AC_SUBST(PYILMBASE_VERSION, 2.0.1)
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR(PyIex/iexmodule.cpp)
AC_CONFIG_HEADER(config/PyIlmBaseConfig.h)
AM_INIT_AUTOMAKE(1.6.3)  dnl Require automake 1.6.3 or better
AM_MAINTAINER_MODE
dnl static python modules make no sense - disable static
AC_DISABLE_STATIC


LIBTOOL_CURRENT=2
LIBTOOL_REVISION=1
LIBTOOL_AGE=0
LIBTOOL_VERSION=$LIBTOOL_CURRENT:$LIBTOOL_REVISION:$LIBTOOL_AGE
AC_SUBST(LIBTOOL_VERSION)

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_LIBTOOL
AC_PROG_MAKE_SET

dnl
dnl PKGCONFIG preparations
dnl

if test -z "${PKG_CONFIG_PATH}"; then
	PKG_CONFIG_PATH=/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:/usr/lib/pkgconfig
fi

export PKG_CONFIG_PATH

dnl
dnl get ccflags and libs from openexr packages, then check 
dnl whether test programs compile
AM_PATH_PKGCONFIG(
   [ILMBASE_CXXFLAGS],
   [ILMBASE_LDFLAGS],
   [ILMBASE_LIBS],
   [IlmBase],
   [OpenEXR],
   [-L/usr/local/lib],
   [-lImath -lHalf -lIex -lIexMath -lIlmThread -lpthread],
   [ilmbase-prefix])

AM_COMPILELINKRUN(
   [IlmBase],
   [ilmbasetest],
   [$ILMBASE_CXXFLAGS],
   [$ILMBASE_LDFLAGS],
   [$ILMBASE_LIBS],[[
#include <stdlib.h>
#include <ImathFun.h>
#ifndef IMATH_NAMESPACE 
#define IMATH_NAMESPACE Imath 
#endif
]],
   [[
   double d = IMATH_NAMESPACE::succd(.23); d+= .2;
   ]],
   AC_MSG_RESULT([Compiled and ran IlmBase test program.]), 
   AC_MSG_ERROR([Could not compile IlmBase test program.]))

dnl Checks for python and boost python
AM_PATH_PYTHON
PYTHON_INC_DIR=`$PYTHON -c 'from distutils.sysconfig import *; print get_python_inc()'`
PYTHON_LIB_DIR=`$PYTHON -c 'from distutils.sysconfig import *; print get_config_var("LIBDIR")'`
LIBS="$LIBS -lpython$PYTHON_VERSION"

BOOST_PYTHON_CXXFLAGS="" 
BOOST_PYTHON_LDFLAGS="" 
BOOST_PYTHON_LIBS="" 
BOOST_PYTHON_LIBNAME="boost_python"

AC_ARG_WITH(
   [boost-include-dir],
   [AS_HELP_STRING([--with-boost-include-dir],["boost include directory"])],
   [BOOST_PYTHON_CXXFLAGS="-I$withval"])

AC_ARG_WITH(
   [boost-lib-dir],
   [AS_HELP_STRING([--with-boost-lib-dir],["boost library directory"])],
   [BOOST_PYTHON_LDFLAGS="-L$withval"])

AC_ARG_WITH(
   [boost-python-libname],
   [AS_HELP_STRING([--with-boost-python-libname],["boost::python library name (default: boost_python)"])],
   [BOOST_PYTHON_LIBNAME="$withval"])

CXXFLAGS="$CXXFLAGS $BOOST_PYTHON_CXXFLAGS -I$PYTHON_INC_DIR"
LDFLAGS="$LDFLAGS $BOOST_PYTHON_LDFLAGS -L$PYTHON_LIB_DIR"

BOOST_PYTHON_LIBS="-l$BOOST_PYTHON_LIBNAME"

AM_COMPILELINKRUN(
   [boost::python],
   [boostpythontest],
   [$CXXFLAGS],
   [$LDFLAGS],
   [$LIBS $BOOST_PYTHON_LIBS],[[
#include <Python.h>
#include <boost/python.hpp>
]],
   [[Py_Initialize(); boost::python::object obj;]],
   AC_MSG_RESULT([Compiled and ran boost::python test program.]), 
   AC_MSG_ERROR([Could not compile boost::python test program.]))

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(limits.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T

AC_MSG_CHECKING(for gcc optimization flags)
old_cflags=$CFLAGS
CFLAGS="$CFLAGS -pipe"
AC_TRY_COMPILE([#include <stdio.h>],
[ printf ("hello, world"); ],
[ EXTRA_OPT_CFLAGS="-pipe"],[ EXTRA_OPT_CFLAGS=""])
CFLAGS=$old_cflags
AC_MSG_RESULT([$EXTRA_OPT_CFLAGS])

dnl Platform-specific stuff
case "$host" in
*linux*)
  AC_DEFINE(HAVE_LINUX_PROCFS)
  ;;
*darwin*) 
  AC_DEFINE(HAVE_DARWIN) 
  ;;
esac

NUMPY_CXXFLAGS=`$PYTHON -c 'from numpy.distutils.misc_util import *; import string; print string.join(@<:@"-I"+x for x in get_numpy_include_dirs()@:>@," ")'`

AC_ARG_WITH([numpy],
  [AS_HELP_STRING([--without-numpy],
    [disable support for numpy])],
  [],
  [with_numpy=yes])

dnl header check not quite working yet
dnl AC_LANG(['C++'])
dnl AS_IF([test "x$with_numpy" != xno],[
dnl   CPPFLAGS="$CPPFLAGS -I$NUMPY_INC_DIR"
dnl   AC_CHECK_HEADER([numpy/arrayobject.h],
dnl     [],
dnl     [AC_MSG_FAILURE([numpy test failed (--without-numpy to disable)])],
dnl     [])
dnl   ])

AM_CONDITIONAL([BUILD_PYIMATHNUMPY], [test "$with_numpy" = yes])

AM_CFLAGS="$EXTRA_OPT_CFLAGS"
AM_CXXFLAGS="$EXTRA_OPT_CFLAGS"
AM_LDFLAGS="$ILMBASE_LDFLAGS $PYTHON_LDFLAGS"

AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_CXXFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(BOOST_PYTHON_LIBS)
AC_SUBST(NUMPY_CXXFLAGS)

AC_CONFIG_FILES([
Makefile
PyIlmBase.pc
config/Makefile
PyIex/Makefile
PyIexTest/Makefile
PyIexTest/pyIexTest
PyImath/Makefile
PyImathTest/Makefile
PyImathTest/pyImathTest
PyImathNumpy/Makefile
])

AC_OUTPUT

AC_MSG_RESULT([
---------------------------------------------
Summary for PyIlmBase features:

Python version                                  $PYTHON_VERSION
boost::python libname                           $BOOST_PYTHON_LIBNAME
---------------------------------------------
])

