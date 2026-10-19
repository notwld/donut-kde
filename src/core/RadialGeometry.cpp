// SPDX-License-Identifier: GPL-2.0-or-later
#include "RadialGeometry.h"

#include <cmath>
#include <numbers>

namespace Donut::RadialGeometry
{
double normalizeAngle(double degrees)
{
    if (!std::isfinite(degrees)) {
        return 0.0;
    }
    double a = std::fmod(degrees, 360.0);
    if (a < 0.0) {
        a += 360.0;
    }
    // fmod can return 360 - epsilon rounding up to exactly 360 after the addition.
    return a >= 360.0 ? 0.0 : a;
}

double angleFromTop(double dx, double dy)
{
    // atan2(x, -y) measures from the negative y axis (up) towards positive x (right),
    // which is clockwise on screen.
    return normalizeAngle(std::atan2(dx, -dy) * 180.0 / std::numbers::pi);
}

double sectorWidth(int count)
{
    return count > 0 ? 360.0 / count : 360.0;
}

double sectorCenterAngle(int sector, int count)
{
    return normalizeAngle(sector * sectorWidth(count));
}

double sectorStartAngle(int sector, int count)
{
    return normalizeAngle(sectorCenterAngle(sector, count) - sectorWidth(count) / 2.0);
}

int sectorForAngle(double angle, int count)
{
    if (count <= 0) {
        return -1;
    }
    const double w = sectorWidth(count);
    const double shifted = normalizeAngle(normalizeAngle(angle) + w / 2.0);
    int index = int(std::floor(shifted / w));
    // Guard against floating point landing exactly on 360.
    return index >= count ? 0 : index;
}

RadialHit hitTest(double dx, double dy, double innerRadius, double outerRadius, int count)
{
    RadialHit hit;
    hit.distance = std::hypot(dx, dy);
    hit.angle = angleFromTop(dx, dy);
    if (hit.distance < innerRadius) {
        hit.region = RadialHit::Region::Hub;
    } else if (hit.distance > outerRadius || count <= 0) {
        hit.region = RadialHit::Region::Outside;
    } else {
        hit.region = RadialHit::Region::Ring;
        hit.sector = sectorForAngle(hit.angle, count);
    }
    return hit;
}
}
