# SPDX-License-Identifier: GPL-2.0-or-later
cmake_minimum_required(VERSION 3.22)

project(donut VERSION 0.1.0 LANGUAGES C CXX)

set(QT_MIN_VERSION "6.6.0")
set(KF_MIN_VERSION "6.0.0")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

set(KDE_COMPILERSETTINGS_LEVEL "6.0")
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMSetupVersion)
include(FeatureSummary)

option(DONUT_BUILD_APP "Build the Donut application" ON)
option(DONUT_BUILD_KWIN_EFFECT "Build the donutdragwatch KWin effect (native drag detection on Plasma Wayland)" ON)
option(DONUT_BUILD_E2E "Build the nested-KWin end-to-end test helpers" ON)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui DBus Concurrent Qml Quick QuickControls2)
find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS CoreAddons Config KIO Archive)

if(DONUT_BUILD_APP)
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED COMPONENTS GlobalAccel WindowSystem DBusAddons)
    find_package(LayerShellQt REQUIRED)
endif()

ecm_setup_version(${PROJECT_VERSION} VARIABLE_PREFIX DONUT VERSION_HEADER "${CMAKE_BINARY_DIR}/donut_version.h")

add_subdirectory(src)

if(DONUT_BUILD_KWIN_EFFECT)
    add_subdirectory(integration/kwin)
endif()

if(BUILD_TESTING)
    find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Test QuickTest)
    add_subdirectory(tests)
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
