dnl
dnl Alternate OpenGL headers (e.g. for Nvidia headers)
dnl

AC_DEFUN([AM_PATH_GL],
[dnl
dnl Get the cflags
dnl
AC_ARG_WITH(gl-includes,[  --with-gl-includes=PFX  Specify which OpenGL headers to use],
	gl_includes="$withval", gl_includes="")
  if test x$gl_includes != x ; then
    GL_CXXFLAGS="-I$gl_includes"
  else
    GL_CXXFLAGS=""
  fi

  AC_SUBST(GL_CXXFLAGS)
])
