# linux specific target definitions

# Privileged KMS capture helper (kms-server-proxy). Standalone binary launched
# via pkexec when Sunshine lacks CAP_SYS_ADMIN; depends only on libdrm so it
# stays minimal and auditable. See src/platform/linux/kms_server_proxy.cpp.
if(SUNSHINE_BUILD_KMS_PROXY)
    add_executable(kms-server-proxy
            "${CMAKE_SOURCE_DIR}/src/platform/linux/kms_server_proxy.cpp")
    target_include_directories(kms-server-proxy SYSTEM PRIVATE ${LIBDRM_INCLUDE_DIRS})
    target_include_directories(kms-server-proxy PRIVATE "${CMAKE_SOURCE_DIR}/src/platform/linux")
    target_link_libraries(kms-server-proxy ${LIBDRM_LIBRARIES})
    set_target_properties(kms-server-proxy PROPERTIES CXX_STANDARD 17)
    add_dependencies(sunshine kms-server-proxy)
endif()
