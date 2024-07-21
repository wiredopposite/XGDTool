cmake_minimum_required(VERSION 3.11)

function(prepend_to_file FILE CONTENT)
    file(READ ${FILE} ORIGINAL_CONTENT)
    file(WRITE ${FILE} "${CONTENT}\n${ORIGINAL_CONTENT}")
endfunction()

set(RPK_DIR ${CMAKE_SOURCE_DIR}/external/Repackinator)
set(ATTACH_XBE_FILE ${RPK_DIR}/attach.xbe)

set(HEADER_DIR ${CMAKE_SOURCE_DIR}/src/generated)
set(ATTACH_XBE_HEADER ${HEADER_DIR}/attach_xbe.h)

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