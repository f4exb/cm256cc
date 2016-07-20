cmake_minimum_required(VERSION 2.8) 

# use, i.e. don't skip the full RPATH for the build tree
set(CMAKE_SKIP_BUILD_RPATH  FALSE)

# when building, don't use the install RPATH already
# (but later on when installing)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE) 

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

# add the automatically determined parts of the RPATH
# which point to directories outside the build tree to the install RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

project(cm256)

if (BUILD_TYPE MATCHES RELEASE)
    set(CMAKE_BUILD_TYPE "Release")
elseif (BUILD_TYPE MATCHES RELEASEWITHDBGINFO)
    set(CMAKE_BUILD_TYPE "ReleaseWithDebugInfo")
elseif (BUILD_TYPE MATCHES DEBUG)
    set(CMAKE_BUILD_TYPE "Debug")
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11" )
add_definitions(-DNO_RESTRICT)

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )

if(${ARCHITECTURE} MATCHES "x86_64|AMD64|x86")
    SET(USE_SIMD "SSSE3" CACHE STRING "Use SIMD SSSE3 instructions")
elseif(${ARCHITECTURE} MATCHES "armv7l")
    SET(USE_SIMD "NEON" CACHE STRING "Use SIMD NEON instructions")
endif()

if(USE_SIMD MATCHES SSSE3)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mssse3" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mssse3" )
        message(STATUS "g++ SSSE3")
        add_definitions(-DUSE_SIMD)
    elseif(MSVC)
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
        set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
        message(STATUS "MSVC SSSE3")
        add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
        add_definitions(-DUSE_SIMD)
    endif()
elseif(USE_SIMD MATCHES NEON)
    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
        set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
        set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
        message(STATUS "g++ NEON")
        add_definitions(-DUSE_NEON)
    endif()
else()
    message(STATUS "Unsupported architecture")
    return()
endif()

set(cm256_SOURCES
  cm256.cpp
  gf256.cpp
)

set(cm256_HEADERS
  cm256.h
  gf256.h
  sse2neon.h
)

add_library(cm256 SHARED
  ${cm256_SOURCES}
)

# single pass test

add_executable(cm256_test
  unit_test/maingcc.cpp
)

target_include_directories(cm256_test PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_test cm256)

# transmit side test

add_executable(cm256_tx
  unit_test/mainutils.cpp
  unit_test/example0.cpp  
  unit_test/transmit.cpp
)

target_include_directories(cm256_tx PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_tx cm256)

# receive side test

add_executable(cm256_rx
  unit_test/mainutils.cpp
  unit_test/example0.cpp 
  unit_test/receive.cpp
)

target_include_directories(cm256_rx PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_rx cm256)

# Installation

install(TARGETS cm256_test cm256_tx cm256_rx DESTINATION bin)
install(TARGETS cm256 DESTINATION lib)
install(FILES ${cm256_HEADERS} DESTINATION include/${PROJECT_NAME})
