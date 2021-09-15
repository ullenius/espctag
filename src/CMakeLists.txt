SET(espctag_src espctag.c)

ADD_EXECUTABLE(espctag ${espctag_src})

TARGET_LINK_LIBRARIES(espctag ${LIBSPCTAG_LIBRARY})

INSTALL(TARGETS espctag DESTINATION "bin")
