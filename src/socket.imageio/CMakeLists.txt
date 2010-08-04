if (NOT Boost_VERSION LESS "103500")
    # Boost < 1.35 is too old to support asio that socket needs
    add_oiio_plugin (socketinput.cpp socketoutput.cpp socket_pvt.cpp)
endif ()

