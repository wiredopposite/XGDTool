set(VCPKG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg)

function(bootstrap_vcpkg)
    message(STATUS "Detected system: ${CMAKE_SYSTEM_NAME}")

    if(EXISTS "${VCPKG_DIR}/vcpkg.exe" OR EXISTS "${VCPKG_DIR}/vcpkg")
        message(STATUS "vcpkg is already bootstrapped.")
    else()
        if(WIN32)
            message(STATUS "Bootstrapping vcpkg on Windows...")
            execute_process(COMMAND ${VCPKG_DIR}/bootstrap-vcpkg.bat WORKING_DIRECTORY ${VCPKG_DIR})
        elseif(UNIX OR APPLE)
            message(STATUS "Bootstrapping vcpkg on Linux/Mac...")
            execute_process(COMMAND ${VCPKG_DIR}/bootstrap-vcpkg.sh WORKING_DIRECTORY ${VCPKG_DIR})
        # elseif(APPLE)
        #     message(STATUS "Bootstrapping vcpkg on macOS...")
        #     execute_process(COMMAND ${VCPKG_DIR}/bootstrap-vcpkg.sh WORKING_DIRECTORY ${VCPKG_DIR})
        else()
            message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
        endif()
    endif()
endfunction()

bootstrap_vcpkg()

execute_process(COMMAND ${VCPKG_DIR}/vcpkg integrate install)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install lz4:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install nlohmann-json:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install cli11:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install zstd:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install openssl:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install curl:x64-windows-static)

# execute_process(COMMAND ${VCPKG_DIR}/vcpkg integrate install)
# execute_process(COMMAND ${VCPKG_DIR}/vcpkg install lz4)
# execute_process(COMMAND ${VCPKG_DIR}/vcpkg install nlohmann-json)
# execute_process(COMMAND ${VCPKG_DIR}/vcpkg install cli11)
# execute_process(COMMAND ${VCPKG_DIR}/vcpkg install zstd)
# execute_process(COMMAND ${VCPKG_DIR}/vcpkg install openssl)

# vcpkg install lz4:x64-windows-static
# vcpkg install zstd:x64-windows-static
# vcpkg install nlohmann-json:x64-windows-static
# vcpkg install cli11:x64-windows-static
# vcpkg install openssl:x64-windows-static
# vcpkg install curl:x64-windows-static


# if(WIN32)
#     execute_process(COMMAND ${VCPKG_DIR}/vcpkg install curl:x64-windows)
# endif()