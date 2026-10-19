/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Minimal input driver for Donut's end-to-end tests.
 *
 * It talks to KWin's org_kde_kwin_fake_input protocol and is ONLY meant to be
 * run against an isolated, nested `kwin_wayland --virtual` instance started by
 * tests/e2e/run-nested-kwin.sh. It is not installed and never used by Donut
 * itself.
 *
 * Commands are read line by line from stdin:
 *   move X Y              absolute pointer move (logical coordinates)
 *   glide X Y STEPS MS    move in STEPS increments over MS milliseconds
 *   down | up             left button press / release
 *   key CODE down|up      evdev key code
 *   sleep MS
 *   # comment
 */
#define _POSIX_C_SOURCE 200809L
#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <wayland-client.h>

#include "fake-input-client-protocol.h"

static struct org_kde_kwin_fake_input *fake_input;
static double cur_x, cur_y;

static void registry_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    (void)data;
    if (strcmp(interface, org_kde_kwin_fake_input_interface.name) == 0) {
        fake_input = wl_registry_bind(registry, name, &org_kde_kwin_fake_input_interface, version < 4 ? version : 4);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name)
{
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove,
};

static void sleep_ms(long ms)
{
    struct timespec ts = {ms / 1000, (ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static void move_to(struct wl_display *display, double x, double y)
{
    org_kde_kwin_fake_input_pointer_motion_absolute(fake_input, wl_fixed_from_double(x), wl_fixed_from_double(y));
    wl_display_flush(display);
    cur_x = x;
    cur_y = y;
}

int main(void)
{
    struct wl_display *display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "fakeinput-driver: cannot connect to WAYLAND_DISPLAY\n");
        return 2;
    }
    struct wl_registry *registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);
    if (!fake_input) {
        fprintf(stderr, "fakeinput-driver: compositor does not offer org_kde_kwin_fake_input\n");
        return 3;
    }
    org_kde_kwin_fake_input_authenticate(fake_input, "donut-e2e", "Donut automated end-to-end test");
    wl_display_roundtrip(display);

    char line[256];
    while (fgets(line, sizeof line, stdin)) {
        double x, y;
        long steps, ms, code;
        char state[16];
        if (line[0] == '#' || line[0] == '\n') {
            continue;
        } else if (sscanf(line, "move %lf %lf", &x, &y) == 2) {
            move_to(display, x, y);
        } else if (sscanf(line, "glide %lf %lf %ld %ld", &x, &y, &steps, &ms) == 4 && steps > 0) {
            const double sx = cur_x, sy = cur_y;
            for (long i = 1; i <= steps; ++i) {
                move_to(display, sx + (x - sx) * i / steps, sy + (y - sy) * i / steps);
                sleep_ms(ms / steps);
            }
        } else if (strncmp(line, "down", 4) == 0) {
            org_kde_kwin_fake_input_button(fake_input, BTN_LEFT, 1);
        } else if (strncmp(line, "up", 2) == 0) {
            org_kde_kwin_fake_input_button(fake_input, BTN_LEFT, 0);
        } else if (sscanf(line, "key %ld %15s", &code, state) == 2) {
            org_kde_kwin_fake_input_keyboard_key(fake_input, (uint32_t)code, strcmp(state, "down") == 0 ? 1 : 0);
        } else if (sscanf(line, "sleep %ld", &ms) == 1) {
            sleep_ms(ms);
        } else {
            fprintf(stderr, "fakeinput-driver: unknown command: %s", line);
            return 4;
        }
        wl_display_roundtrip(display);
    }
    wl_display_roundtrip(display);
    wl_display_disconnect(display);
    return 0;
}
