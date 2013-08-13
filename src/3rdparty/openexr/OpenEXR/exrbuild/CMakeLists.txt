# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrbuild
  exrbuild.cpp
)

TARGET_LINK_LIBRARIES ( exrbuild
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrbuild
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
