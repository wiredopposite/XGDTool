set(VCPKG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external/vcpkg)

function(bootstrap_vcpkg)
    message(STATUS "Bootstrapping vcpkg on Windows...")
    execute_process(COMMAND ${VCPKG_DIR}/bootstrap-vcpkg.bat WORKING_DIRECTORY ${VCPKG_DIR})
endfunction()

bootstrap_vcpkg()

execute_process(COMMAND ${VCPKG_DIR}/vcpkg integrate install)

execute_process(COMMAND ${VCPKG_DIR}/vcpkg install lz4:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install nlohmann-json:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install cli11:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install zstd:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install openssl:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install curl:x64-windows-static)
execute_process(COMMAND ${VCPKG_DIR}/vcpkg install wxwidgets:x64-windows-static)