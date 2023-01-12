cmake_minimum_required(VERSION 3.1)
project (MyWebServer VERSION 0.1.0 LANGUAGES CXX)

# 控制整个项目的C++编译版本
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(IS_DEBUG "debug info something" OFF)
if(IS_DEBUG)
    message(STATUS "DEBUG MODE ON")
    add_compile_definitions(DEBUG_MODE)
    set(CMAKE_BUILD_TYPE "Debug")           # 转为调试模式
else()
    message(STATUS "DEBUG MODE OFF")
endif()

# 添加GDB调试信息
# set(CMAKE_BUILD_TYPE "Debug")  
set(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -g2 -ggdb")  
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3")
message(STATUS "Build type:${CMAKE_BUILD_TYPE}")
message(STATUS "Debug configuration:${CMAKE_CXX_FLAGS_DEBUG}")
message(STATUS "release configuration:${CMAKE_CXX_FLAGS_RELEASE}")

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