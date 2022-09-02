cmake_minimum_required(VERSION 3.0)
project (MyWebServer VERSION 0.1.0 LANGUAGES CXX)

option(IS_DEBUG "debug info something" OFF)
if(IS_DEBUG)
    message(STATUS "DEBUG MODE ON")
    add_compile_definitions(DEBUG_MODE)
else()
    message(STATUS "DEBUG MODE OFF")
endif()


if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Configuring on/for Linux")
else()
    message(STATUS "Configuring on/for Others")
endif()

aux_source_directory(src SRC_SOURCE)
set(OBJECT server)
set(PROJECT_INCLUDE_PATH ${CMAKE_SOURCE_DIR}/include)

if(DEFINED SRC_SOURCE)
    message(STATUS "SRC_SOURCE: ${SRC_SOURCE}")
endif()

add_executable(${OBJECT} main.cpp ${SRC_SOURCE})

target_include_directories(${OBJECT} PUBLIC ${PROJECT_INCLUDE_PATH})

target_link_libraries(${OBJECT} mysqlclient pthread)