# SPDX-License-Identifier: GPL-2.0-or-later

# --- donutcore: everything that can be unit tested without a compositor ------
add_library(donutcore STATIC
    core/ActionValidator.cpp
    core/ActivationTracker.cpp
    core/ArchiveOperations.cpp
    core/ArchiveValidation.cpp
    core/DonutController.cpp
    core/DonutTypes.cpp
    core/DragBackend.cpp
    core/DragPayload.cpp
    core/FileActionService.cpp
    core/FileNaming.cpp
    core/Logging.cpp
    core/RadialGeometry.cpp
    core/SettingsManager.cpp
    core/backends/KWinBackend.cpp
    core/backends/DemoBackend.h
    core/backends/DropTargetBackend.h
)
target_include_directories(donutcore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/core/backends
    ${CMAKE_BINARY_DIR}
)
target_link_libraries(donutcore PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::DBus
    Qt6::Concurrent
    KF6::ConfigCore
    KF6::CoreAddons
    KF6::KIOCore
    KF6::Archive
)

if(DONUT_BUILD_APP)
    add_subdirectory(app)
endif()
