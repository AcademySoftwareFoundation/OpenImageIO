dnl
dnl
dnl compilelinkrun.m4 - used to check whether a required package is properly 
dnl installed.  Compiles, links and runs a c++ test program that uses the 
dnl package to verify that the package is properly installed
dnl
dnl Expected arguments:
dnl $1: the name of the package we are testing, e.g. "OpenEXR"
dnl     used for informational messages, warnings & errors
dnl
dnl $2: the argument passed to configure specifying how to disable this test
dnl     for example:
dnl     $3 = "openexrtest" and 
dnl     "configure --disable-openexrtest" will skip the test
dnl
dnl $3: CXXFLAGS used by the test
dnl
dnl $4: LDFLAGS used by the test
dnl
dnl $5: include section of sourcecode for a c++ test program
dnl $6: body section of sourcecode for a c++ test program
dnl The test program should make use of a library that is supposed to 
dnl be tested.
dnl
dnl $7: the action to be perfomed if the test succeeds 
dnl     (e.g. AC_MSG_RESULT("OpenEXR test program succeeded"))
dnl
dnl $8   the action to be perfomed if the test fails
dnl     (e.g. AC_MSG_ERROR("OpenEXR test program failed"))
dnl

AC_DEFUN([AM_COMPILELINKRUN],
[

dnl create some local m4 "variables" so that we don't have to use numbers
define([arg_pkg_name],$1)
define([arg_disable],$2)
define([arg_cxxflags],$3)
define([arg_ldflags],$4)
define([arg_libs],$5)
define([arg_include_source],$6)
define([arg_body_source],$7)
define([arg_do_yes],$8)
define([arg_do_no],$9)


dnl check arguments
AC_ARG_ENABLE(arg_disable, [  --disable-arg_disable  Do not try to compile and run a test arg_pkg_name program],, enable_programtest=yes)


dnl
dnl if the test hasn't been disabled, then compile, link and run test program
dnl
  if test "x$enable_programtest" = "xyes" ; then

    dnl basic preliminary checks
    AC_MSG_CHECKING(for arg_pkg_name)
    test_runs="yes"

    dnl save settings and setup c++ before we start
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LDFLAGS="$LDFLAGS"
    ac_save_LIBS="$LIBS"
    CXXFLAGS="$CXXFLAGS arg_cxxflags"
    LDFLAGS="$LDFLAGS arg_ldflags"
    LIBS="$LIBS arg_libs"
    AC_REQUIRE_CPP()
    AC_LANG_PUSH([C++])
    rm -f conf.testprogram

    dnl
    dnl first try a complete test - compile, link run
    dnl
    AC_RUN_IFELSE([AC_LANG_PROGRAM(arg_include_source,
                  arg_body_source; [[system("touch conf.testprogram"); ]])],
                  test_runs=yes,
                  test_runs=no,
                  [echo $ac_n "cross compiling; assumed OK... $ac_c"])

    if test "x$test_runs" = "xyes"  || test -f conf.testprogram ; then
       AC_MSG_RESULT(yes)
       ifelse([arg_do_yes], , :, [arg_do_yes])     
    else
       AC_MSG_RESULT(no)
       echo "*** Could not run the arg_pkg_name test program, checking why..."

       test_compiles="yes"
       test_links="yes"

       dnl
       dnl if the program did not run, attempt to compile only
       dnl

       AC_COMPILE_IFELSE([AC_LANG_PROGRAM(arg_include_source,
                                          arg_body_source ; )],
                      test_compiles=yes,
                      test_compiles=no)

       if test "x$test_compiles" = "xno" ; then
          echo "*** The test program could not be compiled.  Is arg_pkg_name installed?"
          echo "*** Check that the cflags (below) includes the arg_pkg_name include directory"

       else
          dnl
          dnl if the program did compile, try linking
          dnl
          AC_LINK_IFELSE([AC_LANG_PROGRAM(arg_include_source,
                                          arg_body_source ; )],
                         test_links=yes,
                         test_links=no)

          if test "x$test_links" = "xyes"; then
              echo "*** The test program compiled and staticly linked, but did not run. This "
              echo "*** usually means that the run-time linker is not finding arg_pkg_name or finding"
              echo "*** the wrong version of arg_pkg_name."
              echo "***"
              echo "*** If the linker is not finding arg_pkg_name, you'll need to set your"
              echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
              echo "*** to the installed location  Also, make sure you have run ldconfig if that"
              echo "*** is required on your system."
          else
              echo "*** The arg_pkg_name test program could be compiled, but could not be dynamically."
              echo "*** or statically linked."
              echo "***"
              echo "*** Make sure the LDFLAGS points to the location of the arg_pkg_name library."
              echo "*** (e.g. -L/usr/local/lib)."
              echo "*** If the run-time linker is not finding arg_pkg_name, you'll need to set your"
              echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
              echo "*** to the installed location  Also, make sure you have run ldconfig if that"
              echo "*** is required on your system."
          fi
       fi

       dnl
       dnl The test failed for some reason. Print out more info, 
       dnl unset flags  and signal an error.
       dnl
       echo "***"
       echo "*** Flags used by the test:"
       echo "***     cflags: $CXXFLAGS "
       echo "***     ldflags: $LDFLAGS"
       echo "***"
       echo "*** You can also run configure with --disable-arg_disable to skip this test."

       ifelse([arg_do_no], , :, [arg_do_no])
    fi

    AC_LANG_POP([C++])
    CXXFLAGS="$ac_save_CXXFLAGS"
    LDFLAGS="$ac_save_LDFLAGS"
    LIBS="$ac_save_LIBS"
        
    dnl
    dnl clean up
    dnl
    rm -f conf.testprogram
  fi


dnl clean up local "variables"
undefine([arg_pkg_name])
undefine([arg_disable])
undefine([arg_cxxflags])
undefine([arg_ldflags])
undefine([arg_libs])
undefine([arg_include_source])
undefine([arg_body_source])
undefine([arg_do_yes])
undefine([arg_do_no])

])
