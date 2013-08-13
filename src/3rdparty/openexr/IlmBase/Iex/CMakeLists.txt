# yue.nicholas@gmail.com

ADD_LIBRARY ( Iex STATIC
  IexBaseExc.cpp
  IexThrowErrnoExc.cpp
  )

INSTALL ( TARGETS
  Iex
  DESTINATION
  lib
)

INSTALL ( FILES 
  IexBaseExc.h
  IexMathExc.h
  IexThrowErrnoExc.h
  IexErrnoExc.h
  IexMacros.h
  Iex.h
  IexNamespace.h
  IexExport.h
  IexForward.h
  DESTINATION
  include/OpenEXR
)
