# pstanczyk@ilm.com

ADD_LIBRARY ( IexMath STATIC
  IexMathFloatExc.cpp  
  IexMathFpu.cpp
  )

INSTALL ( TARGETS
  IexMath
  DESTINATION
  lib
)

INSTALL ( FILES 
  IexMathFloatExc.h  
  IexMathFpu.h  
  IexMathIeeeExc.h
  DESTINATION
  include/OpenEXR
)
