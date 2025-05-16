#include(LibFindMacros)
#libfind_pkg_check_modules(JPEG_PKGCONF libjpegturbo)

message(STATUS "FindJPEG -- Looking for JPEG vars: ${JPEG_MODULE_PATH}")

if(WIN32)
	set(JPEG_INCLUDE_DIRS ${JPEG_MODULE_PATH}/include)
	set(JPEG_LIBRARIES ${JPEG_MODULE_PATH}/lib/jpeg-static.lib)
else()
	set(JPEG_INCLUDE_DIRS ${JPEG_MODULE_PATH}/include)
	set(JPEG_LIBRARIES ${JPEG_MODULE_PATH}/lib64/libjpeg.so)
endif()

message(STATUS "FindJPEG var JPEG_INCLUDE_DIRS: ${JPEG_INCLUDE_DIRS}")
message(STATUS "FindJPEG var JPEG_LIBRARIES: ${JPEG_LIBRARIES}")

set(JPEG_FOUND true)
