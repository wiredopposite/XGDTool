cmake_minimum_required(VERSION 3.11)

if (WIN32)
    set(VCPKG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg)
    include (${CMAKE_SOURCE_DIR}/cmake/setup_vcpkg.cmake)
    set(CMAKE_TOOLCHAIN_FILE ${VCPKG_DIR}/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")
    set(VCPKG_TARGET_TRIPLET x64-windows-static CACHE STRING "Vcpkg target triplet")
endif()

project(XGDTool)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(ENABLE_GUI "Enable GUI" OFF)  # Set to OFF for CLI only

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build (Debug or Release)" FORCE)
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

if(MSVC)
    set(CMAKE_CXX_FLAGS_DEBUG "/Zi /Od /MTd")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /GL /MT")
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/LTCG /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/LTCG /INCREMENTAL:NO")
    set(CMAKE_MODULE_LINKER_FLAGS_RELEASE "/LTCG /INCREMENTAL:NO")
elseif(UNIX)
    set(CMAKE_CXX_STANDARD_LIBRARIES "-L/usr/lib/x86_64-linux-gnu/ -lstdc++")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/embed_resources.cmake)

add_subdirectory(${CMAKE_SOURCE_DIR}/external/ZArchive)

set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

include_directories(
    ${SRC_DIR} 
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

    ${SRC_DIR}/ZARExtractor/ZARExtractor.cpp

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

    ${SRC_DIR}/Utils/EndianUtils.cpp
    ${SRC_DIR}/Utils/StringUtils.cpp

    ${SRC_DIR}/Formats/Xiso.cpp
)

if (ENABLE_GUI)

    list(APPEND SRC
        ${SRC_DIR}/GUI/MainFrame.cpp
        ${SRC_DIR}/GUI/MainFrame_Create.cpp
        ${SRC_DIR}/GUI/MainFrame_Log.cpp
    )

    add_definitions(-DENABLE_GUI)

    if (WIN32)
        add_executable(${PROJECT_NAME} WIN32 ${SRC} resources/resource.rc)
    elseif(UNIX)
        add_executable(${PROJECT_NAME} ${SRC} resources/resource.rc)
    endif()

else()

    add_executable(${PROJECT_NAME} ${SRC})

endif()

if (WIN32)

    find_package(lz4 CONFIG REQUIRED)
    find_package(zstd CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(CLI11 CONFIG REQUIRED)
    find_package(OpenSSL CONFIG REQUIRED)
    find_package(CURL REQUIRED)
    find_package(wxWidgets CONFIG REQUIRED)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        zarchive

        wx::core 
        wx::base

        lz4::lz4
        zstd::libzstd
        CLI11::CLI11
        nlohmann_json::nlohmann_json
        OpenSSL::SSL 
        OpenSSL::Crypto
        CURL::libcurl
    )

    if(MSVC)
        target_compile_options(zarchive PRIVATE /wd4244)
    endif()

elseif(UNIX)

    set(JSON_HPP_URL "https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp")
    set(JSON_DIR "${CMAKE_SOURCE_DIR}/external/nlohmann")
    set(JSON_HPP_PATH "${JSON_DIR}/json.hpp")

    set(CLI11_URL "https://github.com/CLIUtils/CLI11/releases/download/v2.4.2/CLI11.hpp")
    set(CLI11_DIR "${CMAKE_SOURCE_DIR}/external/CLI")
    set(CLI11_HEADER "${CLI11_DIR}/CLI.hpp")

    file(MAKE_DIRECTORY ${JSON_DIR})
    file(MAKE_DIRECTORY ${CLI11_DIR})

    if(NOT EXISTS ${JSON_HPP_PATH})
        file(DOWNLOAD ${JSON_HPP_URL} ${JSON_HPP_PATH})
    endif()

    if(NOT EXISTS ${CLI11_HEADER})
        file(DOWNLOAD ${CLI11_URL} ${CLI11_HEADER} SHOW_PROGRESS)
    endif()

    find_package(PkgConfig REQUIRED)
    set(ENV{PKG_CONFIG_PATH} "/usr/local/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")

    pkg_check_modules(LZ4 REQUIRED lz4)
    pkg_check_modules(ZSTD REQUIRED libzstd)
    find_package(OpenSSL REQUIRED)
    find_package(CURL REQUIRED)
    find_package(wxWidgets REQUIRED COMPONENTS core base)

    include_directories(
        ${CMAKE_SOURCE_DIR}/external
        ${LZ4_INCLUDE_DIRS} 
        ${ZSTD_INCLUDE_DIRS} 
        ${OPENSSL_INCLUDE_DIR} 
        ${CURL_INCLUDE_DIRS} 
        ${wxWidgets_INCLUDE_DIRS}
    )
    link_directories(
        ${LZ4_LIBRARY_DIRS} 
        ${ZSTD_LIBRARY_DIRS} 
        ${OPENSSL_LIBRARIES} 
        ${CURL_LIBRARIES} 
        ${wxWidgets_LIBRARY_DIRS}
    )

    execute_process(
        COMMAND wx-config --cxxflags
        OUTPUT_VARIABLE wxWidgets_CXX_FLAGS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${wxWidgets_CXX_FLAGS} -Wno-ambiguous-reversed-operator")
    include_directories(SYSTEM ${wxWidgets_INCLUDE_DIRS})

    execute_process(
        COMMAND wx-config --libs
        OUTPUT_VARIABLE wxWidgets_LIBS
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    target_link_libraries(${PROJECT_NAME} PRIVATE 
        zarchive 
        ${LZ4_LIBRARIES} 
        ${ZSTD_LIBRARIES} 
        ${OPENSSL_LIBRARIES} 
        ${CURL_LIBRARIES} 
        ${wxWidgets_LIBS}
    )

else()

    message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")

endif()


