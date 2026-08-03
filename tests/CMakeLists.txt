qt_add_executable(waypane_core_tests
    core_tests.cpp
)

target_link_libraries(waypane_core_tests PRIVATE waypane_core Qt6::Test)
add_test(NAME waypane_core_tests COMMAND waypane_core_tests)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
add_test(
    NAME waypane_mcp_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_mcp.py
)
set_tests_properties(waypane_mcp_tests PROPERTIES
    ENVIRONMENT "WAYPANE_MCP=${CMAKE_SOURCE_DIR}/tools/waypane-mcp"
)

add_test(
    NAME waypane_session_logger_tests
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_session_logger.py
)
set_tests_properties(waypane_session_logger_tests PROPERTIES
    ENVIRONMENT "WAYPANE_SESSION_LOGGER=$<TARGET_FILE:waypane-session-logger>"
)

qt_add_executable(waypane_ui_lifecycle_tests
    ui_lifecycle_tests.cpp
    ${CMAKE_SOURCE_DIR}/src/app/connectionlistmodel.cpp
    ${CMAKE_SOURCE_DIR}/src/app/portforwarddialog.cpp
    ${CMAKE_SOURCE_DIR}/src/app/profiledialog.cpp
    ${CMAKE_SOURCE_DIR}/src/app/secretstore.cpp
    ${CMAKE_SOURCE_DIR}/src/app/settingsdialog.cpp
    ${CMAKE_SOURCE_DIR}/src/app/waypanewindow.cpp
)
target_include_directories(waypane_ui_lifecycle_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(waypane_ui_lifecycle_tests PRIVATE
    waypane_core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    KF6::CoreAddons
    KF6::KIOFileWidgets
    KF6::Parts
    KF6::Service
    KF6::Wallet
)
add_test(NAME waypane_ui_lifecycle_tests COMMAND waypane_ui_lifecycle_tests)
# KIO creates Unix-domain sockets below XDG_RUNTIME_DIR. Keep this path short
# enough for the platform socket limit, even when the build directory is deep.
string(MD5 waypane_ui_test_id "${CMAKE_BINARY_DIR}")
string(SUBSTRING "${waypane_ui_test_id}" 0 8 waypane_ui_test_id)
set(waypane_ui_test_home "/tmp/wp-${waypane_ui_test_id}")
file(MAKE_DIRECTORY "${waypane_ui_test_home}" "${waypane_ui_test_home}/runtime")
file(CHMOD "${waypane_ui_test_home}/runtime"
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
)
set_tests_properties(waypane_ui_lifecycle_tests PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${waypane_ui_test_home};XDG_CONFIG_HOME=${waypane_ui_test_home}/config;XDG_RUNTIME_DIR=${waypane_ui_test_home}/runtime"
)
