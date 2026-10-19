# SPDX-License-Identifier: GPL-2.0-or-later
#
# The KWin effect must be compiled against the exact KWin version it runs in:
# the plugin interface id embeds KWIN_PLUGIN_VERSION_STRING and KWin refuses
# mismatching plugins. Rebuild after every KWin upgrade (the Arch PKGBUILD does).

# KWin's exported target needs these before find_package(KWin).
find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS Widgets)
find_package(KWin REQUIRED COMPONENTS kwin)
message(STATUS "Building donutdragwatch effect for KWin ${KWin_VERSION}")

kcoreaddons_add_plugin(donutdragwatch
    SOURCES
        donutdragwatch/main.cpp
        donutdragwatch/donutdragwatcheffect.cpp
        ${PROJECT_SOURCE_DIR}/src/core/ActivationTracker.cpp
    INSTALL_NAMESPACE "kwin/effects/plugins"
)
# The factory class comes from a KWin macro AutoMoc does not know about.
set_property(TARGET donutdragwatch APPEND PROPERTY AUTOMOC_MACRO_NAMES KWIN_EFFECT_FACTORY_SUPPORTED)
target_include_directories(donutdragwatch PRIVATE ${PROJECT_SOURCE_DIR}/src/core)
target_compile_features(donutdragwatch PRIVATE cxx_std_23)
target_link_libraries(donutdragwatch PRIVATE
    KWin::kwin
    Qt6::DBus
    KF6::ConfigCore
)
set_target_properties(donutdragwatch PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins/kwin/effects/plugins")
