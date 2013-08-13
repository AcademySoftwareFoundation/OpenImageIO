# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrstdattr
  main.cpp
)

TARGET_LINK_LIBRARIES ( exrstdattr
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrstdattr
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
