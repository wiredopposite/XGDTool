cmake_minimum_required(VERSION 3.11)

set(VCPKG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg)
include (${CMAKE_SOURCE_DIR}/cmake/setup_vcpkg.cmake)
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(XGDTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(${CMAKE_SOURCE_DIR}/cmake/embed_resources.cmake)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/split_fstream)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/ZArchive)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

include_directories(
    ${SRC_DIR} 
    ${CMAKE_SOURCE_DIR}/generated
)

set(SRC 
    ${SRC_DIR}/main.cpp
    
    ${SRC_DIR}/XGDLog.cpp
    ${SRC_DIR}/XGDException.cpp

    ${SRC_DIR}/ImageReader/ImageReader.cpp
    ${SRC_DIR}/ImageReader/XisoReader/XisoReader.cpp
    ${SRC_DIR}/ImageReader/GoDReader/GoDReader.cpp
    ${SRC_DIR}/ImageReader/CCIReader/CCIReader.cpp
    ${SRC_DIR}/ImageReader/ZARReader/ZARReader.cpp
    # ${SRC_DIR}/ImageReader/CSOReader/CSOReader.cpp

    ${SRC_DIR}/ImageWriter/ImageWriter.cpp
    ${SRC_DIR}/ImageWriter/XisoWriter/XisoWriter.cpp
    ${SRC_DIR}/ImageWriter/GoDWriter/GoDWriter.cpp
    ${SRC_DIR}/ImageWriter/CCIWriter/CCIWriter.cpp
    ${SRC_DIR}/ImageWriter/ZARWriter/ZARWriter.cpp
    ${SRC_DIR}/ImageWriter/CSOWriter/CSOWriter.cpp

    ${SRC_DIR}/ExeTool/ExeTool.cpp

    ${SRC_DIR}/AvlTree/AvlTree.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Calculate.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Traverse.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Create.cpp
    ${SRC_DIR}/AvlTree/AvlIterator.cpp

    # ${SRC_DIR}/InputHelper/InputHelper.cpp
    # ${SRC_DIR}/InputHelper/InputHelper_IO.cpp
    # ${SRC_DIR}/InputHelper/TitleHelper.cpp
    # ${SRC_DIR}/InputHelper/Unity.cpp

    ${SRC_DIR}/Common/EndianUtils.cpp
    ${SRC_DIR}/Common/StringUtils.cpp
    ${SRC_DIR}/Common/IOUtils.cpp

    ${SRC_DIR}/Tests/Tests.cpp
)

add_executable(${PROJECT_NAME} ${SRC})

# if(NOT CMAKE_BUILD_TYPE)
#   set(CMAKE_BUILD_TYPE Release)
# endif()

find_package(lz4 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(CURL REQUIRED)

# set(nlohmann-json_IMPLICIT_CONVERSIONS OFF)

target_link_libraries(${PROJECT_NAME} PRIVATE
    split_fstream
    zarchive
    
    lz4::lz4
    zstd::libzstd
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    OpenSSL::SSL 
    OpenSSL::Crypto
    CURL::libcurl
)

# if (CMAKE_BUILD_TYPE STREQUAL "Debug")
#   message("Build type: Debug")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /Od")
# elseif (CMAKE_BUILD_TYPE STREQUAL "Release")
#   message("Build type: Release")
#   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /O2")
# endif()