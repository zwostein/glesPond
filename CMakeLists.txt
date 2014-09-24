cmake_minimum_required( VERSION 2.8 )
project( glesPond )

set( CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMakeModules" ${CMAKE_MODULE_PATH} )

# configure compiler for c++11 etc.
if( "${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic -std=c++11" )
endif()

include_directories( "${CMAKE_SOURCE_DIR}/src" )


set( EXECUTABLE_NAME "glesPond" )
set( GLESPOND_SOURCES
	src/main.cpp
	src/Shader.cpp
	src/Program.cpp
	src/Error.cpp
)


################################
# Raspberry PI

include( CheckCXXSourceCompiles )
include( CheckCSourceCompiles )

macro( listtostr _LIST _OUTPUT )
	if( ${ARGC} EQUAL 3 )
		# prefix for each element
		set( _LPREFIX ${ARGV2} )
	else()
		set( _LPREFIX "" )
	endif()
	# Do not use string(REPLACE ";" " ") here to avoid messing up list
	# entries
	foreach( _ITEM ${${_LIST}} )
		set( ${_OUTPUT} "${_LPREFIX}${_ITEM} ${${_OUTPUT}}" )
	endforeach()
endmacro()

set( RPI_INCLUDE_DIRS "/opt/vc/include" "/opt/vc/include/interface/vcos/pthreads" "/opt/vc/include/interface/vmcs_host/linux/" )
set( RPI_LIBRARY_DIRS "/opt/vc/lib" )
set( RPI_LIBS bcm_host )
listtostr( RPI_INCLUDE_DIRS RPI_INCLUDE_FLAGS "-I" )
listtostr( RPI_LIBRARY_DIRS RPI_LIBRARY_FLAGS "-L" )

set( CMAKE_REQUIRED_FLAGS "${RPI_INCLUDE_FLAGS} ${RPI_LIBRARY_FLAGS}" )
set( CMAKE_REQUIRED_LIBRARIES "${RPI_LIBS}" )
check_c_source_compiles( "
	#include <bcm_host.h>
	int main(int argc, char **argv) {}"
	IS_RPI )
set( CMAKE_REQUIRED_FLAGS )
set( CMAKE_REQUIRED_LIBRARIES )

if(IS_RPI)
	list( APPEND GLESPOND_LIBRARIES ${RPI_LIBS} )
	list( APPEND CMAKE_INCLUDE_PATH ${RPI_INCLUDE_DIRS} )
	list( APPEND CMAKE_LIBRARY_PATH ${RPI_LIBRARY_DIRS} )
	set( CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${RPI_INCLUDE_FLAGS} ${RPI_LIBRARY_FLAGS}" )
	set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RPI_INCLUDE_FLAGS} ${RPI_LIBRARY_FLAGS}" )
endif()

################################


if( NOT EXISTS "${CMAKE_SOURCE_DIR}/external/SDL2/CMakeLists.txt" )
	set( USE_SYSTEM_SDL2_DEFAULT TRUE )
else()
	message( STATUS "Found local SDL2 in external/SDL2 - Defaulting to NOT use system installation." )
endif()
option( USE_SYSTEM_SDL2 "Use system SDL2 installation" ${USE_SYSTEM_SDL2_DEFAULT} )
if( NOT USE_SYSTEM_SDL2 )
	set( SDL_SHARED FALSE CACHE BOOL "Build a shared version of the library" )
	add_subdirectory( "external/SDL2" )
	include_directories( "${CMAKE_SOURCE_DIR}/external/SDL2/include" )
	list( APPEND GLESPOND_LIBRARIES SDL2-static )
	# wrap the main function for those OSes
	if( ${CMAKE_SYSTEM_NAME} MATCHES "^Windows$|^Android$|^Darwin$" )
		list( APPEND GLESPOND_LIBRARIES SDL2main )
	endif()
else()
	find_package( SDL2 REQUIRED )
	include_directories( ${SDL2_INCLUDE_DIR} )
	list( APPEND GLESPOND_LIBRARIES ${SDL2_LIBRARY} )
endif()

option( GLESPOND_USE_OPENGL_FALLBACK "Use OpenGL instead of OpenGL ES 2" OFF )
if( GLESPOND_USE_OPENGL_FALLBACK )
	find_package( OpenGL REQUIRED )
	include_directories( ${OPENGL_INCLUDE_DIR} )
	list( APPEND GLESPOND_LIBRARIES ${OPENGL_LIBRARIES} )
else()
	find_package( OpenGLES2 REQUIRED )
	include_directories( ${OPENGLES2_INCLUDE_DIR} )
	list( APPEND GLESPOND_LIBRARIES ${OPENGLES2_LIBRARY} )
endif()

find_package( DevIL REQUIRED )
include_directories( ${IL_INCLUDE_DIR} )
list( APPEND GLESPOND_LIBRARIES ${IL_LIBRARIES} )

add_executable( ${EXECUTABLE_NAME} ${GLESPOND_SOURCES} )
target_link_libraries( ${EXECUTABLE_NAME} ${GLESPOND_LIBRARIES} )
install( TARGETS ${EXECUTABLE_NAME} RUNTIME DESTINATION bin )


################################################################
# Packaging

set( CPACK_PACKAGE_VERSION "0.1-snapshot" )

set( CPACK_PACKAGE_DESCRIPTION_SUMMARY "A toy using SDL2 and OpenGL ES 2 to display an interactive pond." )
set( CPACK_PACKAGE_CONTACT "Tobias Himmer <provisorisch@online.de>" )

set( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING.txt" )

set( CPACK_SOURCE_IGNORE_FILES "/\\\\..*$;~$;/build.*/;${CPACK_SOURCE_IGNORE_FILES}" )

set( CPACK_DEBIAN_PACKAGE_DEPENDS "libstdc++6 (>= 4.8.2), libdevil1c2" )
if( USE_SYSTEM_SDL2 )
	set( CPACK_DEBIAN_PACKAGE_DEPENDS "${CPACK_DEBIAN_PACKAGE_DEPENDS}, SDL2" )
endif()

set( CPACK_DEBIAN_PACKAGE_SECTION games )

include( CPack )

################################################################
