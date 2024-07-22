cmake_minimum_required(VERSION 3.11)

set(VCPKG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg)
include (${CMAKE_SOURCE_DIR}/cmake/setup_vcpkg.cmake)
set(CMAKE_TOOLCHAIN_FILE ${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(XGDTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(${CMAKE_SOURCE_DIR}/cmake/embed_resources.cmake)

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

    ${SRC_DIR}/InputHelper/InputHelper.cpp
    ${SRC_DIR}/InputHelper/InputHelper_FS.cpp

    ${SRC_DIR}/ImageReader/ImageReader.cpp
    ${SRC_DIR}/ImageReader/XisoReader/XisoReader.cpp
    ${SRC_DIR}/ImageReader/GoDReader/GoDReader.cpp
    ${SRC_DIR}/ImageReader/CCIReader/CCIReader.cpp
    ${SRC_DIR}/ImageReader/CSOReader/CSOReader.cpp

    ${SRC_DIR}/ImageWriter/ImageWriter.cpp
    ${SRC_DIR}/ImageWriter/XisoWriter/XisoWriter.cpp
    ${SRC_DIR}/ImageWriter/GoDWriter/GoDWriter.cpp
    ${SRC_DIR}/ImageWriter/CCIWriter/CCIWriter.cpp
    ${SRC_DIR}/ImageWriter/ZARWriter/ZARWriter.cpp
    ${SRC_DIR}/ImageWriter/CSOWriter/CSOWriter.cpp

    ${SRC_DIR}/ImageExtractor/ImageExtractor.cpp

    ${SRC_DIR}/Executable/ExeTool.cpp
    ${SRC_DIR}/Executable/AttachXbeTool.cpp

    ${SRC_DIR}/TitleHelper/TitleHelper.cpp

    ${SRC_DIR}/AvlTree/AvlTree.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Calculate.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Traverse.cpp
    ${SRC_DIR}/AvlTree/AvlTree_Create.cpp
    ${SRC_DIR}/AvlTree/AvlIterator.cpp

    ${SRC_DIR}/SplitFStream/SplitIFStream.cpp
    ${SRC_DIR}/SplitFStream/SplitOFStream.cpp

    ${SRC_DIR}/Common/EndianUtils.cpp
    ${SRC_DIR}/Common/StringUtils.cpp
    ${SRC_DIR}/Common/IOUtils.cpp
)

add_executable(${PROJECT_NAME} ${SRC})

find_package(lz4 CONFIG REQUIRED)
find_package(zstd CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CLI11 CONFIG REQUIRED)
find_package(OpenSSL CONFIG REQUIRED)
find_package(CURL REQUIRED)

if(MSVC)
    target_compile_options(zarchive PRIVATE /wd4244)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    zarchive
    
    lz4::lz4
    zstd::libzstd
    CLI11::CLI11
    nlohmann_json::nlohmann_json
    OpenSSL::SSL 
    OpenSSL::Crypto
    CURL::libcurl
)