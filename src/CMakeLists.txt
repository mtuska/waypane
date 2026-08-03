add_library(waypane_core STATIC
    core/connectionprofile.cpp
    core/connectionprofile.h
    core/connectionstore.cpp
    core/connectionstore.h
    core/managedsshconfig.cpp
    core/managedsshconfig.h
    core/opensshconfigimporter.cpp
    core/opensshconfigimporter.h
    core/sshcommandbuilder.cpp
    core/sshcommandbuilder.h
    core/sshruntime.cpp
    core/sshruntime.h
)

target_link_libraries(waypane_core PUBLIC Qt6::Core)
target_include_directories(waypane_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    "${CMAKE_BINARY_DIR}/generated"
)

qt_add_executable(waypane
    app/main.cpp
    app/connectionlistmodel.cpp
    app/connectionlistmodel.h
    app/livecontrolserver.cpp
    app/livecontrolserver.h
    app/portforwarddialog.cpp
    app/portforwarddialog.h
    app/profiledialog.cpp
    app/profiledialog.h
    app/secretstore.cpp
    app/secretstore.h
    app/settingsdialog.cpp
    app/settingsdialog.h
    app/waypanewindow.cpp
    app/waypanewindow.h
)

qt_add_executable(waypane-ssh-helper
    app/sshhelper.cpp
    app/secretstore.cpp
    app/secretstore.h
)

add_executable(waypane-session-logger app/sessionlogger.cpp)
target_link_libraries(waypane-session-logger PRIVATE util)

qt_add_resources(waypane waypane_assets
    PREFIX "/"
    BASE "${CMAKE_SOURCE_DIR}"
    FILES
        "${CMAKE_SOURCE_DIR}/assets/icons/waypane.png"
)

target_link_libraries(waypane PRIVATE
    waypane_core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    KF6::CoreAddons
    KF6::KIOFileWidgets
    KF6::Parts
    KF6::Service
    KF6::Wallet
)

target_link_libraries(waypane-ssh-helper PRIVATE Qt6::Widgets KF6::Wallet)

install(TARGETS waypane RUNTIME DESTINATION bin)
install(TARGETS waypane-ssh-helper RUNTIME DESTINATION libexec)
install(TARGETS waypane-session-logger RUNTIME DESTINATION libexec)

option(WAYPANE_BUILD_PRIVATE_OPENSSH "Build Waypane's isolated OpenSSH runtime" ON)
set(WAYPANE_PRIVATE_OPENSSH_ROOT "" CACHE PATH "Existing path produced by tools/build-private-openssh")
set(waypane_private_openssh_generated OFF)
if(WAYPANE_BUILD_PRIVATE_OPENSSH AND NOT WAYPANE_PRIVATE_OPENSSH_ROOT)
    set(WAYPANE_PRIVATE_OPENSSH_ROOT "${CMAKE_BINARY_DIR}/private-openssh")
    set(waypane_private_openssh_generated ON)
endif()

if(waypane_private_openssh_generated)
    add_custom_command(
        OUTPUT "${WAYPANE_PRIVATE_OPENSSH_ROOT}/.waypane-built"
        COMMAND "${CMAKE_SOURCE_DIR}/tools/build-private-openssh" "${WAYPANE_PRIVATE_OPENSSH_ROOT}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${WAYPANE_PRIVATE_OPENSSH_ROOT}/.waypane-built"
        DEPENDS "${CMAKE_SOURCE_DIR}/tools/build-private-openssh"
        BYPRODUCTS
            "${WAYPANE_PRIVATE_OPENSSH_ROOT}/bin/ssh"
            "${WAYPANE_PRIVATE_OPENSSH_ROOT}/bin/ssh-keygen"
        COMMENT "Building Waypane's checksum-pinned private OpenSSH runtime"
        VERBATIM
    )
    add_custom_target(waypane-private-openssh DEPENDS "${WAYPANE_PRIVATE_OPENSSH_ROOT}/.waypane-built")
endif()

if(WAYPANE_PRIVATE_OPENSSH_ROOT)
    set(waypane_staged_openssh "${CMAKE_CURRENT_BINARY_DIR}/openssh")
    add_custom_command(
        OUTPUT "${waypane_staged_openssh}/.waypane-staged"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${waypane_staged_openssh}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${WAYPANE_PRIVATE_OPENSSH_ROOT}" "${waypane_staged_openssh}"
        COMMAND "${CMAKE_COMMAND}" -E touch "${waypane_staged_openssh}/.waypane-staged"
        DEPENDS "${WAYPANE_PRIVATE_OPENSSH_ROOT}/bin/ssh" "${WAYPANE_PRIVATE_OPENSSH_ROOT}/bin/ssh-keygen"
        COMMENT "Staging Waypane's private OpenSSH runtime beside the development executable"
        VERBATIM
    )
    add_custom_target(waypane-stage-private-openssh ALL DEPENDS "${waypane_staged_openssh}/.waypane-staged")
    if(TARGET waypane-private-openssh)
        add_dependencies(waypane-stage-private-openssh waypane-private-openssh)
    endif()
    add_dependencies(waypane waypane-stage-private-openssh)

    if(NOT EXISTS "${WAYPANE_PRIVATE_OPENSSH_ROOT}/bin/ssh")
        if(NOT waypane_private_openssh_generated)
            message(FATAL_ERROR "WAYPANE_PRIVATE_OPENSSH_ROOT does not contain bin/ssh")
        endif()
    endif()
    install(
        DIRECTORY "${WAYPANE_PRIVATE_OPENSSH_ROOT}/"
        DESTINATION libexec/waypane/openssh
        USE_SOURCE_PERMISSIONS
    )
endif()

foreach(icon_size 16 32 48 64 128 256 512)
    install(
        FILES "${CMAKE_SOURCE_DIR}/assets/icons/hicolor/${icon_size}x${icon_size}/apps/waypane.png"
        DESTINATION "share/icons/hicolor/${icon_size}x${icon_size}/apps"
        RENAME dev.tuska.waypane.png
    )
endforeach()

install(FILES "${CMAKE_SOURCE_DIR}/packaging/dev.tuska.waypane.desktop" DESTINATION share/applications)
install(FILES "${CMAKE_SOURCE_DIR}/packaging/dev.tuska.waypane.metainfo.xml" DESTINATION share/metainfo)
