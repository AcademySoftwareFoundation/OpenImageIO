AC_DEFUN([AM_PATH_PKGCONFIG],
[

dnl sets cflags and ldflags
dnl TEST_CXXFLAGS and TEST_LDFLAGS, by trying thes following
dnl until something works:
dnl
dnl 1 -  try the test_prefix
dnl 2 -  check whether pkgconfig can find values (unless --with-pkg-config=no)
dnl 3 -  use the prefix, if it is not the default
dnl 4 -  use defaults, /usr/local/include/OpenEXR and /usr/local/lib
dnl 
dnl 
dnl Expected arguments
dnl $1: arg_cxxflags - CXXFLAGS variable to set
dnl
dnl $2: arg-ldflags - LDFLAGS variable to set
dnl
dnl $3: package name (the package being checked), as requried by pkg-config
dnl 
dnl $4: arg_include_subdir 
dnl     the name of the subdirectory name that is tacked on to 
dnl     the end of the include path e.g. "OpenEXR" in 
dnl     /usr/local/include/OpenEXR
dnl 
dnl $5: arg_default_libs - default libraries, used if pkgconfig doesnt work
dnl
dnl $6: arg_test_prefix
dnl     the argument passed to configure specifying a directory to
dnl     be used in the CXX and LD flags for example: 
dnl     $2 = "openexr-prefix" and 
dnl     "configure --openexr-prefix=/usr/lib"
dnl     leads to CXX including "-I/usr/lib/OpenEXR"
dnl

dnl create some local m4 "variables" so that we don't have to use numbers
define([arg_cxxflags],$1)
define([arg_ldflags],$2)
define([arg_libs],$3)
define([arg_pkg_name],$4)
define([arg_include_subdir],$5)
define([arg_default_ldflags],$6)
define([arg_default_libs],$7)
define([arg_test_prefix],$8)

TEST_CXXFLAGS=""
TEST_LDFLAGS=""
TEST_LIBS=""

AC_ARG_WITH(arg_test_prefix,[  --with-arg_test_prefix=PFX  Prefix where tested libraries are supposed to be installed (optional)], test_prefix="$withval", test_prefix="NONE")
echo "test_prefix = $test_prefix"

AC_ARG_VAR(PKG_CONFIG, Path to pkg-config command)
AC_PATH_PROG(PKG_CONFIG, pkg-config, no)
AC_ARG_WITH(pkg-config,[  --with-pkg-config=PATH Specify which pkg-config to use (optional)], PKG_CONFIG="$withval",)


if test "x$test_prefix" != "xNONE" ; then
   echo "using arg_test_prefix to set arg_cxxflags, arg_ldflags and arg_libs:"
   for inc_dir in arg_include_subdir
   do
      TEST_CXXFLAGS="$TEST_CXXFLAGS -I$test_prefix/include/$inc_dir"
   done
   TEST_LDFLAGS="-L$test_prefix/lib"
   TEST_LDFLAGS="$TEST_LDFLAGS arg_default_ldflags"
   TEST_LIBS="arg_default_libs"
else
   dnl
   dnl Get the cflags and libraries from the arg_pkg_name package using 
   dnl pkg-config
   dnl
   dnl Note: the TEST_LIBS contains both the -L and the -l flags.  This means
   dnl the -L flags will appear twice on the command line, but we can not
   dnl limit it to --libs-only-l because it may include the "-pthread" flag.
   dnl 
   if test x$PKG_CONFIG != xno ; then
      echo "using pkg-config to set arg_cxxflags and arg_ldflags:"
      TEST_CXXFLAGS="`$PKG_CONFIG --cflags arg_pkg_name`"
      TEST_LDFLAGS="`$PKG_CONFIG --libs-only-L arg_pkg_name`"
      TEST_LIBS="`$PKG_CONFIG --libs arg_pkg_name`"
   else
      echo "Not using pkg-config."
      TEST_CXXFLAGS=""
      TEST_LDFLAGS=""
      TEST_LIBS=""
   fi

   dnl
   dnl if the flags are still not set, try a prefix and finally a default
   dnl
   if test -z "${TEST_CXXFLAGS}"; then
      TEST_CXXFLAGS=""
      if test "x$prefix" != "xNONE"; then
         echo "using prefix to set arg_cxxflags and arg_ldflags:"
         for inc_dir in arg_include_subdir
         do
            TEST_CXXFLAGS="$TEST_CXXFLAGS -I$prefix/include/$inc_dir"
         done
         TEST_LDFLAGS="-L$prefix/lib"
      else
         echo "using default as guess for arg_cxxflags and arg_ldflags:"
         for inc_dir in arg_include_subdir
         do
             TEST_CXXFLAGS="$TEST_CXXFLAGS -I/usr/local/include/$inc_dir"
         done
         TEST_LDFLAGS="arg_default_ldflags"
      fi
      TEST_LIBS="arg_default_libs"
   fi
fi

echo "    arg_cxxflags = $TEST_CXXFLAGS"
echo "    arg_ldflags = $TEST_LDFLAGS"
echo "    arg_libs = $TEST_LIBS"


AC_SUBST(arg_cxxflags, $TEST_CXXFLAGS)
AC_SUBST(arg_ldflags, $TEST_LDFLAGS)
AC_SUBST(arg_libs, $TEST_LIBS)


dnl clean up local "variables"
undefine([arg_cxxflags])
undefine([arg_ldflags])
undefine([arg_libs])
undefine([arg_pkg_name])
undefine([arg_include_subdir])
undefine([arg_default_ldflags])
undefine([arg_default_libs])
undefine([arg_test_prefix])

])


