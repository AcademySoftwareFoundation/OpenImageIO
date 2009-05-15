#########################################################################
# The top-level Makefile is just a stub that merely includes
# src/make/master.mk
#########################################################################

include src/make/master.mk

$(info "dist_dir = ${dist_dir}")

cmakesetup:
	- ${MKDIR} build/${platform}
	( cd build/${platform} ; \
	  cmake -DCMAKE_INSTALL_PREFIX=${working_dir}/dist/${platform} \
		-DBOOST_ROOT=${BOOST_HOME} \
		../../src )

cmake: cmakesetup
	( cd build/${platform} ; make )

cmakeinstall: cmake
	( cd build/${platform} ; make install )

