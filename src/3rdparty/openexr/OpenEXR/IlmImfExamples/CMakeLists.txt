# yue.nicholas@gmail.com

ADD_EXECUTABLE ( IlmImfExamples
  drawImage.cpp
  generalInterfaceExamples.cpp
  generalInterfaceTiledExamples.cpp
  lowLevelIoExamples.cpp
  main.cpp
  previewImageExamples.cpp
  rgbaInterfaceExamples.cpp
  rgbaInterfaceTiledExamples.cpp
 )

TARGET_LINK_LIBRARIES ( IlmImfExamples
  IlmImf
  Iex
  IlmThread
  Half
  ${PTHREAD_LIB}
  ${Z_LIB}
  )
