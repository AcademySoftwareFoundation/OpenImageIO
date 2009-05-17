#########################################################################
# The top-level Makefile is just a stub that merely includes
# src/make/master.mk
#########################################################################

include src/make/master.mk

$(info "dist_dir = ${dist_dir}")

OIIO_MAKE_FLAGS ?=
OIIO_CMAKE_FLAGS ?=

VERBOSE := ${SHOWCOMMANDS}
ifneq (${VERBOSE},)
OIIO_MAKE_FLAGS += VERBOSE=${VERBOSE}
endif

ifneq (${EMBEDPLUGINS},)
OIIO_CMAKE_FLAGS += -DEMBEDPLUGINS:BOOL=${EMBEDPLUGINS}
endif

ifneq (${USE_OPENGL},)
OIIO_CMAKE_FLAGS += -DUSE_OPENGL:BOOL=${USE_OPENGL}
endif

ifneq (${USE_QT},)
OIIO_CMAKE_FLAGS += -DUSE_QT:BOOL=${USE_QT}
endif

ifdef DEBUG
OIIO_CMAKE_FLAGS += -DCMAKE_BUILD_TYPE:STRING=Debug
endif

$(info OIIO_CMAKE_FLAGS = ${OIIO_CMAKE_FLAGS})
$(info OIIO_MAKE_FLAGS = ${OIIO_MAKE_FLAGS})



cmakesetup:
	- ${MKDIR} build/${platform}
	( cd build/${platform}${variant} ; \
	  cmake -DCMAKE_INSTALL_PREFIX=${working_dir}/dist/${platform}${variant} \
		-DBOOST_ROOT=${BOOST_HOME} \
		${OIIO_CMAKE_FLAGS} \
		../../src )

cmake: cmakesetup
	( cd build/${platform}${variant} ; make ${OIIO_MAKE_FLAGS} )

cmakeinstall: cmake
	( cd build/${platform}${variant} ; make ${OIIO_MAKE_FLAGS} install )

package: cmakeinstall
	( cd build/${platform}${variant} ; make ${OIIO_MAKE_FLAGS} package )

package_source: cmakeinstall
	( cd build/${platform}${variant} ; make ${OIIO_MAKE_FLAGS} package_source )
