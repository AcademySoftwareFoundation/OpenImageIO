set (public_headers argparse.h dassert.h errorhandler.h export.h 
                    filesystem.h fmath.h hash.h
                    imagebuf.h imagecache.h imageio.h osdep.h paramlist.h
                    refcnt.h strutil.h sysutil.h texture.h thread.h timer.h
                    typedesc.h ustring.h varyingref.h
                    colortransfer.h
    )

install (FILES ${public_headers} DESTINATION include/OpenImageIO
         COMPONENT developer)

if (USE_TBB)
    file (GLOB tbb_headers tbb/*.h)
    file (GLOB tbb_headers2 tbb/machine/*.h)
    install (FILES ${tbb_headers} DESTINATION include/OpenImageIO/tbb
             COMPONENT developer)
    install (FILES ${tbb_headers2} DESTINATION include/OpenImageIO/tbb/machine
             COMPONENT developer)
endif ()

