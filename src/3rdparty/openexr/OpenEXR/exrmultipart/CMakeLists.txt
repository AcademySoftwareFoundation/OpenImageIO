# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrmultipart
  exrmultipart.cpp
)

TARGET_LINK_LIBRARIES ( exrmultipart
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrmultipart
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
