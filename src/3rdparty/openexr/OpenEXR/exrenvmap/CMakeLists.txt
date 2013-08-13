# yue.nicholas@gmail.com

ADD_EXECUTABLE ( exrenvmap
  makeLatLongMap.cpp
  readInputImage.cpp
  resizeImage.cpp
  makeCubeMap.cpp
  main.cpp
  blurImage.cpp
  EnvmapImage.cpp
)

TARGET_LINK_LIBRARIES ( exrenvmap
  IlmImf
  IlmThread
  Iex
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
)

INSTALL ( TARGETS
  exrenvmap
  DESTINATION
  ${CMAKE_INSTALL_PREFIX}/bin
)
