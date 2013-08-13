# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrmakepreview
  makePreview.cpp
  main.cpp
)

TARGET_LINK_LIBRARIES ( exrmakepreview
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrmakepreview
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
