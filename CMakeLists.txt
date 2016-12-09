PROJECT(deviceaccess-DoocsBackend)
cmake_minimum_required(VERSION 2.8)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/Modules)

set(${PROJECT_NAME}_MAJOR_VERSION 00)
set(${PROJECT_NAME}_MINOR_VERSION 02)
set(${PROJECT_NAME}_PATCH_VERSION 00)
include(cmake/set_version_numbers.cmake)

#Moderate version of the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -fPIC -Wextra -ansi -Wuninitialized")

#use -DCMAKE_BUILD_TYPE=Debug in your cmake command to turn on the coverage option
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 --coverage")

# linker flags
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--export-dynamic")

include(cmake/add_dependency.cmake)
add_dependency(mtca4u-deviceaccess 00.12 REQUIRED)
add_dependency(DOOCS 18.10.5 COMPONENTS server REQUIRED)
add_dependency(doocs-server-test-helper 00.04 REQUIRED)

FIND_PACKAGE(Boost COMPONENTS thread system unit_test_framework REQUIRED)
include_directories(${CMAKE_SOURCE_DIR}/include)
set(${PROJECT_NAME}_INCLUDE_DIRS ${${PROJECT_NAME}_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/include/)
ENABLE_TESTING()

aux_source_directory(${CMAKE_SOURCE_DIR}/src library_sources)

# Create the executables for automated unit testing.
# Currently we assume that they are all tests contained in one file, so
# each file gives a new executable. This section has to be adapted if this should change.
# loop over all sources files in execultables_src directory
aux_source_directory(${CMAKE_SOURCE_DIR}/tests/executables_src testExecutables)
foreach( testExecutableSrcFile ${testExecutables})
  # NAME_WE means the base name without path and (longest) extension
  get_filename_component(excutableName ${testExecutableSrcFile} NAME_WE)
  # each test includes a DOOCS server
  add_executable(${excutableName} ${testExecutableSrcFile} ${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer/doocsDummy_rpc_server.cc ${CMAKE_SOURCE_DIR}/tests/DoocsDummyServer/eq_dummy.cc)
  set_target_properties(${excutableName} PROPERTIES COMPILE_FLAGS "${mtca4u-deviceaccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
  set_target_properties(${excutableName} PROPERTIES LINK_FLAGS ${DOOCS_LINK_FLAGS})
  target_link_libraries(${excutableName} ${doocs-server-test-helper_LIBRARIES} ${mtca4u-deviceaccess_LIBRARIES} ${DOOCS_LIBRARIES} ${Boost_LIBRARIES} ${PROJECT_NAME})
  add_test(${excutableName} ${excutableName})
  # copy config file
  FILE( COPY ${CMAKE_SOURCE_DIR}/tests/doocsDummy_rpc_server.conf DESTINATION ${PROJECT_BINARY_DIR})
  FILE( RENAME ${PROJECT_BINARY_DIR}/doocsDummy_rpc_server.conf ${PROJECT_BINARY_DIR}/${excutableName}.conf)
endforeach( testExecutableSrcFile )

FILE( COPY ${CMAKE_SOURCE_DIR}/tests/dummies.dmap DESTINATION ${PROJECT_BINARY_DIR})

#The make coverage command is only available in debug mode
IF(CMAKE_BUILD_TYPE STREQUAL "Debug")
  configure_file(cmake/Makefile.coverage.in
    ${PROJECT_BINARY_DIR}/Makefile.coverage @ONLY)
  add_custom_target(coverage
    make -f Makefile.coverage
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating test coverage documentation" VERBATIM
    )
ENDIF(CMAKE_BUILD_TYPE STREQUAL "Debug")

# C++ library
add_library(${PROJECT_NAME} SHARED ${library_sources} )
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${${PROJECT_NAME}_VERSION} SOVERSION ${${PROJECT_NAME}_SOVERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${mtca4u-deviceaccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
target_link_libraries(${PROJECT_NAME} ${mtca4u-deviceaccess_LIBRARIES} ${DOOCS_LIBRARIES} ${Boost_LIBRARIES})

#configure a header file which contains the version information for use in C++
#configure_file(cmake/version.h.in "${PROJECT_BINARY_DIR}/version.h" @ONLY)

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
if(DOXYGEN_FOUND)
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/Doxyfile.in.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in @ONLY)
  configure_file(${CMAKE_CURRENT_BINARY_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
  add_custom_target(doc ALL
    ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating API documentation with Doxygen" VERBATIM
    )
  install(DIRECTORY ${CMAKE_BINARY_DIR}/doc DESTINATION .)
else(DOXYGEN_FOUND)
  message("Doxygen not found, documentation will not be build.")
endif(DOXYGEN_FOUND)


#Install the library and the executables
install(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
	    LIBRARY DESTINATION lib
  )

#all include files go into include/PROJECT_NAME
#The exclusion of ${PROJECT_NAME} prevents the recursive installation of the files just being installed.
#The original headers are in include/*.h, the installed ones in include/PROJECT_NAME/*.h.
install(DIRECTORY ${${PROJECT_NAME}_INCLUDE_DIRS} DESTINATION include/${PROJECT_NAME}
          FILES_MATCHING PATTERN "*.h"
	  PATTERN ".svn" EXCLUDE
	  PATTERN "${PROJECT_NAME}" EXCLUDE
	  )

#Create the config files by filling the correct variables into the template (*.cmake.in).
#All variables @VARIABLE@ are replaced with the current values, but they have to be set first....
#Only the reference header is needed. The other variables are for dependencies of this library,
#but there are none.
set(${PROJECT_NAME}_INCLUDE_DIRS "${CMAKE_INSTALL_PREFIX}/include ${Boost_INCLUDE_DIRS}")
set(${PROJECT_NAME}_LIBRARIES "${PROJECT_NAME} ${mtca4u-deviceaccess_LIBRARIES} ${Boost_LIBRARIES}")
set(${PROJECT_NAME}_LIBRARY_DIRS "${CMAKE_INSTALL_PREFIX}/lib ${mtca4u-deviceaccess_LIBRARY_DIRS} ${Boost_LIBRARY_DIRS}")
set(${PROJECT_NAME}_CPPFLAGS "${mtca4u-deviceaccess_CXX_FLAGS} ${DOOCS_CXX_FLAGS}")
set(${PROJECT_NAME}_LDFLAGS "")
include(${CMAKE_SOURCE_DIR}/cmake/create_cmake_config_files.cmake)
