cmake_minimum_required(VERSION 3.11)

function(prepend_to_file FILE CONTENT)
    file(READ ${FILE} ORIGINAL_CONTENT)
    file(WRITE ${FILE} "${CONTENT}\n${ORIGINAL_CONTENT}")
endfunction()

set(DL_DIR ${CMAKE_SOURCE_DIR}/external/Repackinator)
set(REPACK_JSON_FILE ${DL_DIR}/RepackList.json)
set(ATTACH_XBE_FILE ${DL_DIR}/attach.xbe)
set(LICENSE_FILE ${DL_DIR}/LICENSE.md)

set(REPACK_ATTACH_XBE_URL "https://raw.githubusercontent.com/Team-Resurgent/Repackinator/main/Repackinator/Resources/attach.xbe")
set(REPACK_LIST_URL "https://raw.githubusercontent.com/Team-Resurgent/Repackinator/main/RepackList.json")
set(REPACK_LICENSE_URL "https://raw.githubusercontent.com/Team-Resurgent/Repackinator/main/LICENSE.md")

if(NOT EXISTS ${REPACK_JSON_FILE} OR NOT EXISTS ${ATTACH_XBE_FILE} OR NOT EXISTS ${LICENSE_FILE})
    file(MAKE_DIRECTORY ${DL_DIR})
    message("Downloading Repackinator files...")
    file(DOWNLOAD ${REPACK_LIST_URL} ${REPACK_JSON_FILE})
    file(DOWNLOAD ${REPACK_ATTACH_XBE_URL} ${ATTACH_XBE_FILE})
    file(DOWNLOAD ${REPACK_LICENSE_URL} ${LICENSE_FILE})
endif()

set(HEADER_DIR ${CMAKE_SOURCE_DIR}/src/generated)
set(ATTACH_XBE_HEADER ${HEADER_DIR}/attach_xbe.h)
set(REPACK_LIST_HEADER ${HEADER_DIR}/repack_list.h)

if (NOT EXISTS ${HEADER_DIR})
    file(MAKE_DIRECTORY ${HEADER_DIR})
endif()

include(${CMAKE_SOURCE_DIR}/external/cmake-bin2h/bin2h.cmake)

if (NOT EXISTS ${ATTACH_XBE_HEADER})
    message("Generating header from xbe...")
    bin2h(  
        SOURCE_FILE ${ATTACH_XBE_FILE} 
        HEADER_FILE ${ATTACH_XBE_HEADER} 
        VARIABLE_NAME ATTACH_XBE 
    )
    prepend_to_file(${ATTACH_XBE_HEADER} "#ifndef _ATTACH_XBE_H_\n#define _ATTACH_XBE_H_\n")
    file(APPEND ${ATTACH_XBE_HEADER} "#endif // _ATTACH_XBE_H_\n")
endif()

if (NOT EXISTS ${REPACK_LIST_HEADER})
    message("Generating header from json, this will take a few minutes...")
    bin2h(  
        SOURCE_FILE ${REPACK_JSON_FILE} 
        HEADER_FILE ${REPACK_LIST_HEADER} 
        VARIABLE_NAME REPACK_LIST 
    )
    prepend_to_file(${REPACK_LIST_HEADER} "#ifndef _REPACK_LIST_H_\n#define _REPACK_LIST_H_\n")
    file(APPEND ${REPACK_LIST_HEADER} "#endif // _REPACK_LIST_H_\n")
endif()