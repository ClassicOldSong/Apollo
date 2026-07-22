# unix specific packaging
# put anything here that applies to both linux and macos

# return here if building a macos package
if(SUNSHINE_PACKAGE_MACOS)
    return()
endif()

# Installation destination dir
set(CPACK_SET_DESTDIR true)
if(NOT CMAKE_INSTALL_PREFIX)
    set(CMAKE_INSTALL_PREFIX "/usr/share/sunshine")
endif()

install(TARGETS sunshine RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

# The KMS capture helper must live next to the sunshine binary; the client
# resolves it relative to its own executable path.
if(SUNSHINE_BUILD_KMS_PROXY AND TARGET kms-server-proxy)
    install(TARGETS kms-server-proxy RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
