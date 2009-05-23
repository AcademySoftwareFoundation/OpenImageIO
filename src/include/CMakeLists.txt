set (public_headers dassert.h export.h filesystem.h hash.h
                    imagebuf.h imagecache.h imageio.h paramlist.h
                    refcnt.h strutil.h texture.h thread.h
                    typedesc.h ustring.h varyingref.h
    )

install (FILES ${public_headers} DESTINATION include/OpenImageIO)
