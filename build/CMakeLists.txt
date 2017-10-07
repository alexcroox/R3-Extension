# Run CMake with
# Windows 32bit: `cmake . -G "Visual Studio 14 2015"`
# Windows 64bit: `cmake . -G "Visual Studio 14 2015 Win64"`
# Linux 32bit: `cmake . -DCMAKE_C_FLAGS=-m32 -DCMAKE_CXX_FLAGS=-m32`
# Linux 64bit: `cmake . -DCMAKE_C_FLAGS=-m64 -DCMAKE_CXX_FLAGS=-m64`

# Set this to where the MySQL 32 bit C Connecter was installed 
SET(MYSQL32_HOME "c:/Program Files (x86)/MySQL/MySQL Connector C 6.1")
# Set this to where the MySQL 64 bit C Connecter was installed
SET(MYSQL64_HOME "c:/Program Files/MySQL/MySQL Connector C 6.1")

cmake_minimum_required (VERSION 3.2)
PROJECT (r3_extension)

SET(SOURCES
    ../include/config.h
    ../include/extension.h
    ../include/log.h
    ../include/sql.h
    ../include/os.h
    ../src/config.cpp
    ../src/extension.cpp
    ../src/log.cpp
    ../src/sql.cpp
    ../src/main.cpp
    ../src/os.cpp
)

IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
    SET(MYSQL_HOME ${MYSQL64_HOME})
    SET(R3_LIB_NAME "r3_extension_x64")
ELSE()
    SET(MYSQL_HOME ${MYSQL32_HOME})
    SET(R3_LIB_NAME "r3_extension")
ENDIF()

IF (MSVC)
    # Use static runtime
    foreach(flag_var
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
       if(${flag_var} MATCHES "/MD")
          string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
       endif(${flag_var} MATCHES "/MD")
    endforeach(flag_var)
ENDIF()

INCLUDE_DIRECTORIES(../include)
INCLUDE_DIRECTORIES(${MYSQL_HOME}/include)

LINK_DIRECTORIES(${MYSQL_HOME}/lib)

# Console
ADD_EXECUTABLE(r3_extension_console ${SOURCES})
TARGET_COMPILE_DEFINITIONS(r3_extension_console PRIVATE R3_CONSOLE)

# Lib
ADD_LIBRARY(${R3_LIB_NAME} SHARED ${SOURCES})

IF (MSVC)
    SET(EXTRA_LIBS)
    SET(STATIC_LIBS
        ${MYSQL_HOME}/lib/vs14/mysqlclient.lib
    )
ELSE()
    SET(THREADS_PREFER_PTHREAD_FLAG ON)
    FIND_PACKAGE(Threads REQUIRED)

    SET_PROPERTY(TARGET r3_extension_console PROPERTY CXX_STANDARD 11)
    SET_PROPERTY(TARGET ${R3_LIB_NAME} PROPERTY CXX_STANDARD 11)

    SET(EXTRA_LIBS dl Threads::Threads)
    SET(STATIC_LIBS
        ${MYSQL_HOME}/lib/libmysqlclient.a
    )
ENDIF()

TARGET_LINK_LIBRARIES(r3_extension_console ${STATIC_LIBS} ${EXTRA_LIBS})
TARGET_LINK_LIBRARIES(${R3_LIB_NAME} ${STATIC_LIBS} ${EXTRA_LIBS})
