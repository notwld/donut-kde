# SPDX-License-Identifier: GPL-2.0-or-later
# Helpers for tests/e2e/run-nested-kwin.sh. Never installed.
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)

set(_xml ${CMAKE_CURRENT_SOURCE_DIR}/fake-input.xml)
set(_hdr ${CMAKE_CURRENT_BINARY_DIR}/fake-input-client-protocol.h)
set(_src ${CMAKE_CURRENT_BINARY_DIR}/fake-input-protocol.c)
add_custom_command(OUTPUT ${_hdr} ${_src}
    COMMAND ${WAYLAND_SCANNER} client-header ${_xml} ${_hdr}
    COMMAND ${WAYLAND_SCANNER} private-code ${_xml} ${_src}
    DEPENDS ${_xml})

add_executable(donut-e2e-fakeinput fakeinput-driver.c ${_src} ${_hdr})
target_include_directories(donut-e2e-fakeinput PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(donut-e2e-fakeinput PRIVATE PkgConfig::WaylandClient m)
set_target_properties(donut-e2e-fakeinput PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_test(NAME e2e_nested_kwin COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/run-nested-kwin.sh ${CMAKE_BINARY_DIR})
set_tests_properties(e2e_nested_kwin PROPERTIES LABELS "e2e" SKIP_RETURN_CODE 77 TIMEOUT 180 RUN_SERIAL TRUE)
