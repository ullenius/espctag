CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(espctag)

SET(LIBSPCTAG_MINIMUM_REQUIRED 0.2)

SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules/")
FIND_PACKAGE(libspctag REQUIRED)
IF (LIBSPCTAG_FOUND)
	MESSAGE(STATUS "libspctag found")
ELSE (LIBSPCTAG_FOUND)
	MESSAGE(FATAL_ERROR "libspctag not found!")
ENDIF (LIBSPCTAG_FOUND)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(doc)