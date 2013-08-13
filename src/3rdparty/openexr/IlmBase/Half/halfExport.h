#ifndef HALFEXPORT_H
#define HALFEXPORT_H

//
//  Copyright (c) 2008 Lucasfilm Entertainment Company Ltd.
//  All rights reserved.   Used under authorization.
//  This material contains the confidential and proprietary
//  information of Lucasfilm Entertainment Company and
//  may not be copied in whole or in part without the express
//  written permission of Lucasfilm Entertainment Company.
//  This copyright notice does not imply publication.
//

#if defined(PLATFORM_WINDOWS)
#  if defined(PLATFORM_BUILD_STATIC)
#    define PLATFORM_EXPORT_DEFINITION 
#    define PLATFORM_IMPORT_DEFINITION
#  else
#    define PLATFORM_EXPORT_DEFINITION __declspec(dllexport) 
#    define PLATFORM_IMPORT_DEFINITION __declspec(dllimport)
#  endif
#else   // linux/macos
#  if defined(PLATFORM_VISIBILITY_AVAILABLE)
#    define PLATFORM_EXPORT_DEFINITION __attribute__((visibility("default")))
#    define PLATFORM_IMPORT_DEFINITION
#  else
#    define PLATFORM_EXPORT_DEFINITION 
#    define PLATFORM_IMPORT_DEFINITION
#  endif
#endif

#if defined(HALF_EXPORTS)                          // create library
#  define HALF_EXPORT PLATFORM_EXPORT_DEFINITION
#else                                              // use library
#  define HALF_EXPORT PLATFORM_IMPORT_DEFINITION
#endif

#endif // #ifndef HALFEXPORT_H

