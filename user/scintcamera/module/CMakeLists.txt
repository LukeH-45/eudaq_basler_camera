include_directories(include)
find_package(Pylon QUIET)
if(NOT ${Pylon_FOUND})
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindPylon.cmake)
endif()
include_directories(${Pylon_INCLUDE_DIRS})
link_libraries(${Pylon_LIBRARIES})
aux_source_directory(src MODULE_SRC)

if(NOT EUDAQ_LCIO_LIBRARY)
  list(REMOVE_ITEM MODULE_SRC src/NiRawEvent2LCEventConverter.cc)
endif()

if(NOT USER_DUMMY_BUILD)
  list(REMOVE_ITEM MODULE_SRC src/*.cc)
endif()

add_library(${EUDAQ_MODULE} SHARED ${MODULE_SRC})
target_link_libraries(${EUDAQ_MODULE} ${EUDAQ_CORE_LIBRARY}
  ${EUDAQ_LCIO_LIBRARY} ${LCIO_LIBRARIES})

install(TARGETS
  ${EUDAQ_MODULE}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)
