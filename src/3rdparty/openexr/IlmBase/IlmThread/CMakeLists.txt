# yue.nicholas@gmail.com

IF (WIN32)
  SET( ILMTHREAD_LIBRARY_SOURCES
    IlmThread.cpp
    IlmThreadMutex.cpp
    IlmThreadMutexPosix.cpp
    IlmThreadMutexWin32.cpp
    IlmThreadPool.cpp
    IlmThreadPosix.cpp
    IlmThreadSemaphore.cpp
    IlmThreadSemaphorePosix.cpp
    IlmThreadSemaphorePosixCompat.cpp
    IlmThreadSemaphoreWin32.cpp
    IlmThreadWin32.cpp
	)
ELSE ()

  SET( ILMTHREAD_LIBRARY_SOURCES
    IlmThread.cpp
    IlmThreadMutex.cpp
    IlmThreadMutexPosix.cpp
    IlmThreadPool.cpp
    IlmThreadPosix.cpp
    IlmThreadSemaphore.cpp
    IlmThreadSemaphorePosixCompat.cpp
    IlmThreadSemaphorePosix.cpp
 )

ENDIF()

ADD_LIBRARY ( IlmThread STATIC
  ${ILMTHREAD_LIBRARY_SOURCES}
  )

INSTALL ( TARGETS
  IlmThread
  DESTINATION
  lib
)

INSTALL ( FILES 
  IlmThreadPool.h
  IlmThread.h
  IlmThreadSemaphore.h
  IlmThreadMutex.h
  IlmThreadNamespace.h
  IlmThreadExport.h
  IlmThreadForward.h
  DESTINATION
  include/OpenEXR
)
