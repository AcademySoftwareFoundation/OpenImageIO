
dnl
dnl Cg support
dnl

AC_DEFUN([AM_PATH_CG],
[dnl
dnl Get the cflags
dnl
AC_ARG_WITH(cg-prefix,[  --with-cg-prefix=PFX  Prefix where Cg is installed (optional)],
	    cg_prefix="$withval", cg_prefix="")

  if test x$cg_prefix != x ; then
    CG_CXXFLAGS="-I$cg_prefix/include"
    CG_LDFLAGS="-L$cg_prefix/lib -lGL -lCg -lCgGL -lGLU -lpthread -lglut"
  else
    case $host_os in
      darwin*)
        CG_CXXFLAGS=""
        CG_LDFLAGS="-framework Cg -framework AGL -framework OpenGL -framework GLUT"
        ;;
      *)
        CG_CXXFLAGS=""
        CG_LDFLAGS="-lGL -lCg -lCgGL -lGLU -lpthread -lglut"
        ;;
    esac
  fi

  AC_MSG_CHECKING(for Cg)
  no_cg=""

  ac_save_CXXFLAGS="$CXXFLAGS"
  ac_save_LDFLAGS="$LDFLAGS"
  CXXFLAGS="$CXXFLAGS $CG_CXXFLAGS"
  LDFLAGS="$CG_LDFLAGS"

  case $host_os in
    darwin*)
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_LINK([
    #include <OpenGL/gl.h>
    #include <Cg/cg.h>
    #include <Cg/cgGL.h>],
    [
        cgCreateContext ();
    ],, no_cg=yes)
    ;;
  *)
      AC_LANG_SAVE
      AC_LANG_CPLUSPLUS
      AC_TRY_LINK([
    #include <GL/gl.h>
    #include <Cg/cg.h>
    #include <Cg/cgGL.h>],
    [
        cgCreateContext ();
    ],, no_cg=yes)
    ;;
  esac

  AC_LANG_RESTORE
  CXXFLAGS="$ac_save_CXXFLAGS"
  LDFLAGS="$ac_save_LDFLAGS"

  if test "x$no_cg" = "x" ; then
    AC_MSG_RESULT(yes)
      ifelse([$1], , :, [$1])
  else
    AC_MSG_RESULT(no)
    echo "*** The Cg test program could not be compiled."
    echo "*** Possible reasons:"
    echo "***     - The Cg libraries and includes are not installed."
    echo "***     - configure cannot find Cg (use the"
    echo "***       --with-cg-prefix option to tell configure where"
    echo "***       to find it)."
    echo "***     - Your version of Cg is out of date.  Please update it"
    echo "***       to the latest version."
    echo "***"
    echo "*** The exrdisplay program will not be built with fragment shader"
    echo "*** support because the fragment shader support depends on Cg."
    CG_CXXFLAGS=""
    CG_LDFLAGS=""
    ifelse([$2], , :, [$2])
  fi
  AC_SUBST(CG_CXXFLAGS)
  AC_SUBST(CG_LDFLAGS)
])
  
