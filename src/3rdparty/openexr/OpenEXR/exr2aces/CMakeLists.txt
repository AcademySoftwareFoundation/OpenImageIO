# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exr2aces
  main.cpp
)

TARGET_LINK_LIBRARIES ( exr2aces
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exr2aces
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
