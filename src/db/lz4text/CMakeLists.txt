set(INCROOT ${CMAKE_SOURCE_DIR}/src/db/lz4text)
set(SRCROOT ${CMAKE_SOURCE_DIR}/src/db/lz4text)

# all source files
set(SRC
  ${SRCROOT}/main.cpp
  ${INCROOT}/lz4text_db.hpp
  ${SRCROOT}/lz4text_db.cpp
  ${CMAKE_SOURCE_DIR}/deps/lz4/lz4.c
  ${CMAKE_SOURCE_DIR}/deps/lz4/lz4.h
  )

include_directories("${CMAKE_SOURCE_DIR}/deps/lz4")

# define db text target
add_library(db_lz4text SHARED ${SRC})
target_link_libraries(db_lz4text Medusa)
set_target_properties(db_lz4text PROPERTIES FOLDER "Databases")

if(WIN32)
  install(TARGETS db_lz4text RUNTIME DESTINATION .)
else()
  install(TARGETS db_lz4text LIBRARY DESTINATION .)
endif()
