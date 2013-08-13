# yue.nicholas@gmail.com

ADD_LIBRARY ( Imath STATIC
  ImathBox.cpp
  ImathRandom.cpp
  ImathColorAlgo.cpp
  ImathShear.cpp
  ImathFun.cpp
  ImathVec.cpp
  ImathMatrixAlgo.cpp
)

INSTALL ( TARGETS
  Imath
  DESTINATION
  lib
)

INSTALL ( FILES 
  ImathColorAlgo.h
  ImathMatrixAlgo.h
  ImathVec.h
  ImathShear.h
  ImathFun.h
  ImathBox.h
  ImathBoxAlgo.h
  ImathEuler.h
  ImathExc.h
  ImathLimits.h
  ImathLine.h
  ImathLineAlgo.h
  ImathMatrix.h
  ImathPlane.h
  ImathSphere.h
  ImathVecAlgo.h
  ImathQuat.h
  ImathFrustum.h
  ImathMath.h
  ImathGL.h
  ImathColor.h
  ImathRandom.h
  ImathRoots.h
  ImathHalfLimits.h
  ImathInterval.h
  ImathGLU.h
  ImathFrame.h
  ImathPlatform.h
  ImathInt64.h
  ImathForward.h
  ImathNamespace.h
  ImathExport.h
  DESTINATION
  include/OpenEXR
)
