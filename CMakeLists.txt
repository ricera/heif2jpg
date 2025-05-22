cmake_minimum_required(VERSION 3.21)

project(heif2jpg LANGUAGES CXX)

include(ExternalProject)

# Version number
set (PACKAGE_VERSION 0.0.1)

set(THIRD_PARTY_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# At some point...
if(NOT MSVC)
    # Add FreeBSD compile flags here
endif()

# I don't know how necessary these are
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# For clang
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# find_package(JPEG)
# find_package(HEIF)

# Libheif setup parameters

#find_package(LIBDE265)
#if(NOT LIBDE265_FOUND)
    # set(LIBDE265_LIBRARY_ARG "")
    # set(LIBDE265_INCLUDE_DIR_ARG "")
#endif()
set(LIBHEIF_CONFIG "Release")

# Find JPEG library on system
#if(NOT JPEG_FOUND)
#    message(FATAL_ERROR "Could not find JPEG")
#else()
#    message(STATUS "Found JPEG: ${JPEG_LIBRARIES} (found version \"${JPEG_VERSION}\")")
#endif()

# if(NOT HEIF_FOUND)
    set(LIBHEIF_TARGET_NAME libheif)
    set(LIBHEIF_PREFIX_DIR ${CMAKE_CURRENT_BINARY_DIR}/${LIBHEIF_TARGET_NAME})
    set(LIBHEIF_SOURCE_DIR ${THIRD_PARTY_DIR}/${LIBHEIF_TARGET_NAME})
    set(LIBHEIF_BINARY_DIR ${LIBHEIF_PREFIX_DIR}/src/${LIBHEIF_TARGET_NAME}-build)
    set(LIBHEIF_INCLUDE_DIRS ${LIBHEIF_SOURCE_DIR}/libheif/api/libheif ${LIBHEIF_BINARY_DIR})

    # Copied from libultrahdr
    set(HEIF_LIB ${CMAKE_STATIC_LIBRARY_PREFIX}heif${CMAKE_STATIC_LIBRARY_SUFFIX})
    set(HEIF_LIB_PREFIX ${LIBHEIF_BINARY_DIR}/libheif/${LIBHEIF_CONFIG}/)
    set(LIBHEIF_LIBRARIES ${HEIF_LIB_PREFIX}${HEIF_LIB})

    message(STATUS "LIBHEIF_INCLUDE_DIRS: ${LIBHEIF_INCLUDE_DIRS}")
    message(STATUS "HEIF_LIB: ${HEIF_LIB}")
    message(STATUS "HEIF_LIB_PREFIX: ${HEIF_LIB_PREFIX}")
    message(STATUS "LIBHEIF_LIBRARIES: ${LIBHEIF_LIBRARIES}")

    # CONFIGURE_COMMAND cmake ${LIBHEIF_SOURCE_DIR} --preset=release-noplugins -DWITH_EXAMPLES=OFF -DLIBDE265_LIBRARIES=${LIBDE_LIBRARY} -DLIBDE265_INCLUDE_DIRS=${LIBDE_INCLUDE_DIR}
    ExternalProject_Add(${LIBHEIF_TARGET_NAME}
        GIT_REPOSITORY https://github.com/strukturag/libheif.git
        GIT_TAG d84f58fe0af319f01ec2fd1739873f10400253b5
        PREFIX ${LIBHEIF_PREFIX_DIR}
        SOURCE_DIR ${LIBHEIF_SOURCE_DIR}
        BINARY_DIR ${LIBHEIF_BINARY_DIR}
        CMAKE_ARGS --preset=release-noplugins -DWITH_EXAMPLES=OFF -DLIBDE265_LIBRARY=${LIBDE265_LIBRARY_ARG} -DLIBDE265_INCLUDE_DIR=${LIBDE265_INCLUDE_DIR_ARG}
        # CMAKE_ARGS --preset=release-noplugins -DWITH_EXAMPLES=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --config ${LIBHEIF_CONFIG}
        # BUILD_BYPRODUCTS ${LIBHEIF_LIBRARIES}
        INSTALL_COMMAND ""
    )
# endif()

# Need to figure out what these do
set(PRIVATE_INCLUDE_DIR ${LIBHEIF_SOURCE_DIR} ${LIBHEIF_INCLUDE_DIRS})
set(PRIVATE_LINK_LIBS ${LIBHEIF_LIBRARIES})

set(HEIF2JPG_APP heif2jpg)
add_executable(${HEIF2JPG_APP}
    "app/main.cc"
)
target_include_directories(${HEIF2JPG_APP} PRIVATE ${PRIVATE_INCLUDE_DIR})
target_link_libraries(${HEIF2JPG_APP} PRIVATE ${PRIVATE_LINK_LIBS})
