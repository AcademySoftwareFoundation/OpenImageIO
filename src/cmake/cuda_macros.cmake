# Copyright Contributors to the OpenImageIO project.
# SPDX-License-Identifier: Apache-2.0
# https://github.com/AcademySoftwareFoundation/OpenImageIO


set_option (OIIO_USE_CUDA "Include Cuda support if found" OFF)
set_cache (CUDA_TARGET_ARCH "sm_60" "CUDA GPU architecture (e.g. sm_60)")
set_cache (CUDAToolkit_ROOT "" "Path to CUDA toolkit")

if (OIIO_USE_CUDA)
    set (CUDA_PROPAGATE_HOST_FLAGS ON)
    set (CUDA_VERBOSE_BUILD ${VERBOSE})
    checked_find_package(CUDAToolkit
                         VERSION_MIN 9.0
                         RECOMMEND_MIN 11.0
                         RECOMMEND_MIN_REASON
                            "We don't actively test CUDA older than 11"
                         )
    list (APPEND CUDA_NVCC_FLAGS ${CSTD_FLAGS} -expt-relaxed-constexpr)
    if (CUDAToolkit_FOUND)
        add_compile_definitions (OIIO_USE_CUDA=1)
    endif ()
endif ()


# Add necessary ingredients to make `target` include and link against Cuda.
function (oiio_cuda_target target)
    if (CUDAToolkit_FOUND)
        target_link_libraries (${target} PRIVATE
                               CUDA::cudart_static
                              )
    endif ()
endfunction()
