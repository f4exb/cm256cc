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

project(cm256cc)

if (BUILD_TYPE MATCHES RELEASE)
    set(CMAKE_BUILD_TYPE "Release")
elseif (BUILD_TYPE MATCHES RELEASEWITHDBGINFO)
    set(CMAKE_BUILD_TYPE "ReleaseWithDebugInfo")
elseif (BUILD_TYPE MATCHES DEBUG)
    set(CMAKE_BUILD_TYPE "Debug")
else()
    set(CMAKE_BUILD_TYPE "Release")
endif()

# Compiler flags.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fmax-errors=10 -O3 -ffast-math -ftree-vectorize ${EXTRA_FLAGS}")
set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11" )
set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11" )
add_definitions(-DNO_RESTRICT)

##############################################################################

EXECUTE_PROCESS( COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE )
message( STATUS "Architecture: ${ARCHITECTURE}" )

if (${ARCHITECTURE} MATCHES "x86_64|AMD64|x86|i686")
    EXECUTE_PROCESS( COMMAND grep flags /proc/cpuinfo OUTPUT_VARIABLE CPU_FLAGS )
    if (${CPU_FLAGS} MATCHES "avx2")
        set(HAS_AVX2 ON CACHE BOOL "Architecture has AVX2 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mavx2" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mavx2" )
            message(STATUS "Use g++ AVX2 SIMD instructions")
            add_definitions(-DUSE_AVX2)
        endif()
    else()
        set(HAS_AVX2 OFF CACHE BOOL "Architecture does not have AVX2 SIMD enabled")
    endif()
    if (${CPU_FLAGS} MATCHES "sse4_1")
        set(HAS_SSE4_1 ON CACHE BOOL "Architecture has SSE 4.1 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -msse4.1" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -msse4.1" )
            message(STATUS "Use g++ SSE 4.1 SIMD instructions")
            add_definitions(-DUSE_SSE4_1)
        endif()
    else()
        set(HAS_SSE4_1 OFF CACHE BOOL "Architecture does not have SSE 4.1 SIMD enabled")
    endif()
    if (${CPU_FLAGS} MATCHES "ssse3")
        set(HAS_SSSE3 ON CACHE BOOL "Architecture has SSSE3 SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mssse3" )
            message(STATUS "Use g++ SSSE3 SIMD instructions")
            add_definitions(-DUSE_SSSE3)
        elseif(MSVC)
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /arch:SSSE3" )
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Oi /GL /Ot /Ox /arch:SSSE3" )
            set( CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG" )
            message(STATUS "Use MSVC SSSE3 SIMD instructions")
            add_definitions (/D "_CRT_SECURE_NO_WARNINGS")
            add_definitions(-DUSE_SSSE3)
        endif()
    else()
        set(HAS_SSSE3 OFF CACHE BOOL "Architecture does not have SSSE3 SIMD enabled")
    endif()
elseif (${ARCHITECTURE} MATCHES "armv7l")
    EXECUTE_PROCESS( COMMAND grep Features /proc/cpuinfo OUTPUT_VARIABLE CPU_FLAGS )
    if (${CPU_FLAGS} MATCHES "neon")
        set(HAS_NEON ON CACHE BOOL FORCE "Architecture has NEON SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            set( CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -mfpu=neon" )
            set( CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -mfpu=neon" )
            message(STATUS "Use g++ NEON SIMD instructions")
            add_definitions(-DUSE_NEON)
        endif()
    else()
        set(HAS_NEON OFF CACHE BOOL FORCE "Architecture does not have NEON SIMD enabled")
    endif()
elseif (${ARCHITECTURE} MATCHES "aarch64")
    EXECUTE_PROCESS( COMMAND grep Features /proc/cpuinfo OUTPUT_VARIABLE CPU_FLAGS )
    if (${CPU_FLAGS} MATCHES "asimd")
        set(HAS_NEON ON CACHE BOOL FORCE "Architecture has NEON SIMD enabled")
        if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX)
            message(STATUS "Aarch64 always has NEON SIMD instructions")
            add_definitions(-DUSE_NEON)
        endif()
    else()
        set(HAS_NEON OFF CACHE BOOL FORCE "Architecture does not have NEON SIMD enabled")
    endif()
endif()

##############################################################################

if(HAS_SSSE3)
    message(STATUS "Architecture supports SSSE3 - OK")
elseif(HAS_NEON)
    message(STATUS "Architecture supports Neon - OK")
else()
    message(STATUS "Unsupported architecture - Terminated")
    return()
endif()

find_package(Boost)

set(cm256_SOURCES
  cm256.cpp
  gf256.cpp
)

set(cm256_HEADERS
  cm256.h
  gf256.h
  sse2neon.h
)

add_library(cm256cc SHARED
  ${cm256_SOURCES}
)

# single pass test

add_executable(cm256_test
  unit_test/cm256_test.cpp
)

target_include_directories(cm256_test PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_test cm256cc)

# transmit side test

add_executable(cm256_tx
  unit_test/mainutils.cpp
  unit_test/UDPSocket.cpp
  unit_test/example0.cpp  
  unit_test/example1.cpp  
  unit_test/transmit.cpp
)

target_include_directories(cm256_tx PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_tx cm256cc)

# receive side test

add_executable(cm256_rx
  unit_test/mainutils.cpp
  unit_test/UDPSocket.cpp
  unit_test/example0.cpp 
  unit_test/example1.cpp  
  unit_test/receive.cpp
)

target_include_directories(cm256_rx PUBLIC
    ${PROJECT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_link_libraries(cm256_rx cm256cc)

# Installation

install(TARGETS cm256_test cm256_tx cm256_rx DESTINATION bin)
install(TARGETS cm256cc DESTINATION lib${LIB_SUFFIX})
install(FILES ${cm256_HEADERS} DESTINATION include/${PROJECT_NAME})
