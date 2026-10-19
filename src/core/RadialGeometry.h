// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

/*
 * Coordinate convention used everywhere in Donut:
 *
 *  - Input is a delta (dx, dy) from the donut centre in screen coordinates,
 *    i.e. y grows downwards.
 *  - Angles are in degrees, 0° is 12 o'clock and angles grow CLOCKWISE,
 *    normalised to [0, 360).
 *  - Sector i is centred on i * (360 / count). Sector 0 therefore spans
 *    [360 - w/2, 360) ∪ [0, w/2) where w = 360 / count, so it straddles the
 *    0/360 seam. A point exactly on a boundary belongs to the sector that
 *    starts there (the next one clockwise).
 *  - distance < innerRadius          -> Hub (never selects an action)
 *  - innerRadius <= d <= outerRadius -> Ring
 *  - distance > outerRadius          -> Outside
 */
namespace Donut
{
struct RadialHit {
    enum class Region {
        Hub,
        Ring,
        Outside,
    };
    Region region = Region::Outside;
    int sector = -1; ///< valid only for Region::Ring
    double angle = 0.0;
    double distance = 0.0;
};

namespace RadialGeometry
{
double normalizeAngle(double degrees);
double angleFromTop(double dx, double dy);
double sectorWidth(int count);
double sectorCenterAngle(int sector, int count);
double sectorStartAngle(int sector, int count);
int sectorForAngle(double angle, int count);
RadialHit hitTest(double dx, double dy, double innerRadius, double outerRadius, int count);
}
}
