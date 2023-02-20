get_filename_component(EUDAQ_USERNAME ${CMAKE_CURRENT_LIST_DIR} NAME)
set(EUDAQ_MODULE module_${EUDAQ_USERNAME})

option(USER_SCINTCAM_BUILD "build user/SCINTCAMERA" ON)

if(NOT USER_SCINTCAM_BUILD)
    message(STATUS "user/scintcamera is NOT to be built (USER_SCINTCAM_BUILD=OFF)")
  return()
endif()

message(STATUS "user/scintcamera is to be built (USER_SCINTCAM_BUILD=ON)")
add_subdirectory(module)
