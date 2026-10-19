#pragma once
// General polygonal cross-section property engine (Phase 7S). Computes
// area, centroid, second moments of area (Iy, Iz, Iyz) and, from those,
// principal axes/moments, for an arbitrary section built from one or
// more simple polygon contours -- an outer boundary plus any number of
// holes.
//
// Method: Green's-theorem polygon integration (the standard "shoelace"
// moment formulas, e.g. Bourke, "Polygon Area and Centroid of a
// Polygon"), written directly in this project's own local (y,z) section
// axes (Beam3D/Section.hpp convention: Iy = integral of z^2 dA, resists
// x-z plane bending; Iz = integral of y^2 dA, resists x-y plane bending)
// rather than translating a generic (x,y) textbook formula at the call
// site, to avoid a y/z mislabeling bug. For one contour with vertices
// (y_i,z_i), i=0..n-1 (closed, vertex n == vertex 0) and cross term
// c_i = y_i*z_{i+1} - y_{i+1}*z_i:
//   A       = (1/2)  * sum(c_i)
//   A*Cy    = (1/6)  * sum((y_i+y_{i+1}) * c_i)    (NOT 1/12 -- a transcription slip here once
//                                                    produced centroids off by exactly 2x,
//                                                    invisible in symmetric test cases and only
//                                                    caught by an explicit translation test)
//   A*Cz    = (1/6)  * sum((z_i+z_{i+1}) * c_i)
//   Izz_O   = (1/12) * sum((y_i^2 + y_i*y_{i+1} + y_{i+1}^2) * c_i)   (about the ORIGIN; this is Iz)
//   Iyy_O   = (1/12) * sum((z_i^2 + z_i*z_{i+1} + z_{i+1}^2) * c_i)   (about the ORIGIN; this is Iy)
//   Iyz_O   = (1/24) * sum((y_i*z_{i+1} + 2*y_i*z_i + 2*y_{i+1}*z_{i+1} + y_{i+1}*z_i) * c_i)
// A simple closed polygon traversed counter-clockwise (in the y-z plane)
// gives positive A; clockwise gives negative A. A section with holes is
// built by giving the outer boundary CCW and every hole boundary CW
// (opposite winding): the signed quantities above then simply SUM across
// contours -- correct for area, first moments and the origin-referenced
// second moments without special-casing, by the linearity Green's
// theorem gives for a union/difference of regions. The origin-based
// second moments are summed first and shifted to the COMBINED centroid
// exactly once at the end (shifting per-contour first and summing again
// would double-count the parallel-axis correction).
#include "../core/Types.hpp"
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace kopruq::sections {

// A point in the section's local (y,z) plane, NOT (x,y) -- x() returns local y, y() returns local z.
using Point2 = Eigen::Vector2d;
using Contour = std::vector<Point2>; // one simple closed polygon; need not repeat the first vertex at the end.

struct PolygonSectionProperties {
  Real area{};
  Real Cy{}, Cz{};                 // centroid, in the input coordinate system
  Real Iyy{}, Izz{}, Iyz{};        // about the CENTROID; Iyy==this project's Iy, Izz==this project's Iz
  Real I1{}, I2{};                 // principal second moments (I1 >= I2)
  Real principal_angle_rad{};      // angle from local y to the I1 principal axis, CCW positive
  // Extreme fiber distances from the centroid (exact for a polygon: the true extremes of a
  // straight-edged boundary are always at a vertex) and the section moduli they imply.
  Real cy_plus{}, cy_minus{}, cz_plus{}, cz_minus{}; // distances to +y, -y, +z, -z extreme fibers
  Real Wz_plus{}, Wz_minus{}, Wy_plus{}, Wy_minus{}; // Wz = Izz/cy (resists y-bending), Wy = Iyy/cz
};

namespace detail {
struct ContourSums { Real A{}, ACy{}, ACz{}, Izz_O{}, Iyy_O{}, Iyz_O{}; };

inline ContourSums contourSums(const Contour& c) {
  if (c.size() < 3) throw std::invalid_argument("a section contour needs at least 3 vertices");
  ContourSums s;
  const std::size_t n = c.size();
  for (std::size_t i = 0; i < n; ++i) {
    const Real y0 = c[i].x(), z0 = c[i].y();
    const Real y1 = c[(i + 1) % n].x(), z1 = c[(i + 1) % n].y();
    const Real cross = y0 * z1 - y1 * z0;
    s.A += cross;
    s.ACy += (y0 + y1) * cross;
    s.ACz += (z0 + z1) * cross;
    s.Izz_O += (y0*y0 + y0*y1 + y1*y1) * cross;
    s.Iyy_O += (z0*z0 + z0*z1 + z1*z1) * cross;
    s.Iyz_O += (y0*z1 + 2*y0*z0 + 2*y1*z1 + y1*z0) * cross;
  }
  s.A /= 2; s.ACy /= 6; s.ACz /= 6; s.Izz_O /= 12; s.Iyy_O /= 12; s.Iyz_O /= 24;
  return s;
}
} // namespace detail

// contours: outer boundary wound CCW, any holes wound CW (opposite winding), in local (y,z).
inline PolygonSectionProperties computePolygonSection(const std::vector<Contour>& contours) {
  if (contours.empty()) throw std::invalid_argument("a section needs at least one contour");
  detail::ContourSums total;
  for (const auto& c : contours) {
    auto s = detail::contourSums(c);
    total.A += s.A; total.ACy += s.ACy; total.ACz += s.ACz;
    total.Izz_O += s.Izz_O; total.Iyy_O += s.Iyy_O; total.Iyz_O += s.Iyz_O;
  }
  if (!(std::abs(total.A) > 1e-15)) throw std::invalid_argument("section net area is zero or negative; check contour winding (outer CCW, holes CW)");
  PolygonSectionProperties p;
  p.area = total.A;
  p.Cy = total.ACy / total.A;
  p.Cz = total.ACz / total.A;
  // Parallel axis theorem, shifting from the origin to the combined centroid, exactly once.
  p.Izz = total.Izz_O - total.A * p.Cy * p.Cy;
  p.Iyy = total.Iyy_O - total.A * p.Cz * p.Cz;
  p.Iyz = total.Iyz_O - total.A * p.Cy * p.Cz;
  // Principal axes/moments (standard 2D closed form, equivalent to Mohr's circle).
  const Real avg = (p.Iyy + p.Izz) / 2, diff = (p.Iyy - p.Izz) / 2;
  const Real R = std::sqrt(diff*diff + p.Iyz*p.Iyz);
  p.I1 = avg + R; p.I2 = avg - R;
  p.principal_angle_rad = 0.5 * std::atan2(-2 * p.Iyz, p.Iyy - p.Izz);

  // Extreme fiber distances: exact for a polygon, since every vertex lies on the (straight-edged)
  // boundary and the true extreme of such a boundary is always at a vertex.
  Real yMin = contours.front().front().x(), yMax = yMin, zMin = contours.front().front().y(), zMax = zMin;
  for (const auto& c : contours) for (const auto& v : c) {
    yMin = std::min(yMin, v.x()); yMax = std::max(yMax, v.x());
    zMin = std::min(zMin, v.y()); zMax = std::max(zMax, v.y());
  }
  p.cy_plus = yMax - p.Cy; p.cy_minus = p.Cy - yMin;
  p.cz_plus = zMax - p.Cz; p.cz_minus = p.Cz - zMin;
  p.Wz_plus = p.Izz / p.cy_plus; p.Wz_minus = p.Izz / p.cy_minus;
  p.Wy_plus = p.Iyy / p.cz_plus; p.Wy_minus = p.Iyy / p.cz_minus;
  return p;
}

} // namespace kopruq::sections
