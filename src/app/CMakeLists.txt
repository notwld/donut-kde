# SPDX-License-Identifier: GPL-2.0-or-later
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets)

add_subdirectory(${PROJECT_SOURCE_DIR}/qml ${CMAKE_BINARY_DIR}/qml)

add_library(donutapp STATIC
    Autostart.cpp
    Diagnostics.cpp
    DonutApplication.cpp
    IconProvider.cpp
    OverlayWindow.cpp
    ShortcutManager.cpp
)
target_include_directories(donutapp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(donutapp PUBLIC
    donutcore
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    KF6::GlobalAccel
    KF6::WindowSystem
    LayerShellQt::Interface
)

add_executable(donut main.cpp)
target_link_libraries(donut PRIVATE donutapp donutqmlplugin KF6::DBusAddons)
install(TARGETS donut ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})

install(FILES ${PROJECT_SOURCE_DIR}/assets/org.kde.donut.desktop DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES ${PROJECT_SOURCE_DIR}/assets/icons/org.kde.donut.svg DESTINATION ${KDE_INSTALL_ICONDIR}/hicolor/scalable/apps)
install(FILES ${PROJECT_SOURCE_DIR}/integration/dolphin/donut-actions.desktop DESTINATION ${KDE_INSTALL_DATADIR}/kio/servicemenus
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
