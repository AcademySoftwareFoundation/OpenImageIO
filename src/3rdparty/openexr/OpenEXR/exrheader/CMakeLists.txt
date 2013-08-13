# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrheader
  main.cpp
)

TARGET_LINK_LIBRARIES ( exrheader
  IlmImf
  Iex
  IlmThread
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrheader
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
