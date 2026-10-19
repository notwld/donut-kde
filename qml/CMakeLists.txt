# SPDX-License-Identifier: GPL-2.0-or-later
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(donutqml
    URI org.kde.donut
    VERSION 1.0
    STATIC
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/org/kde/donut
    QML_FILES
        Theme.qml
        ActionIcon.qml
        DonutCenter.qml
        DonutMenu.qml
        DonutSegment.qml
        Main.qml
        SettingsPage.qml
        StandaloneWindow.qml
        ToastNotification.qml
    # C++ types live in org.kde.donut.core and are registered at runtime.
    NO_LINT
)

qt_add_resources(donutqml "donut_icons"
    PREFIX /icons
    BASE ${PROJECT_SOURCE_DIR}/assets/icons
    FILES
        ${PROJECT_SOURCE_DIR}/assets/icons/archive-extract.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/archive-insert.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/dialog-cancel.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/dialog-error.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/dialog-ok-apply.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/edit-copy.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/edit-move.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/go-down.svg
        ${PROJECT_SOURCE_DIR}/assets/icons/org.kde.donut.svg
)

target_link_libraries(donutqml PRIVATE Qt6::Quick)
