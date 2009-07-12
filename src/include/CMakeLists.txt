set (public_headers argparse.h dassert.h export.h filesystem.h fmath.h hash.h
                    imagebuf.h imagecache.h imageio.h osdep.h paramlist.h
                    refcnt.h strutil.h sysutil.h texture.h thread.h timer.h
                    typedesc.h ustring.h varyingref.h
    )

install (FILES ${public_headers} DESTINATION include/OpenImageIO
         COMPONENT developer)
