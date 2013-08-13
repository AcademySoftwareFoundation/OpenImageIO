# yue.nicholas@gmail.com

ADD_EXECUTABLE ( IlmImfFuzzTest
  fuzzFile.cpp
  main.cpp
  testFuzzScanLines.cpp
  testFuzzTiles.cpp
  )

TARGET_LINK_LIBRARIES ( IlmImfFuzzTest IlmImf Iex Imath Half IlmThread ${PTHREAD_LIB} ${Z_LIB})

ADD_TEST ( TestIlmImfFuzz IlmImfFuzzTest )
