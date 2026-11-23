#pragma once
// Section outlines for the model view (2026-11-23, user request: show members with their section
// shapes). Display data only -- never used by the solver. Each outline is a list of closed polygons in
// section coordinates (y horizontal = MIDAS B direction / member local y, z vertical = MIDAS H
// direction / member local z, z up): the first is the outer boundary (counter-clockwise), any further
// ones are holes (clockwise). centroidalOutline() shifts them so that the centroid -- the member axis
// -- is at the origin, using the same exact polygon integration as the section properties.
//
// The outlines follow the same dimension conventions as MidasSectionLibrary.hpp (they are checked
// against it: outline area == section area, and the top fiber == ResolvedSectionProperties::
// topFiber, see tests/unit/midas_section_outline_tests.cpp). Display simplifications, stated here:
// web-to-flange fillets (H r1) are not drawn; circular parts are polygons (kCircleSegments);
// the channel's web and the angle's vertical leg are drawn on the -y side (MIDAS's orientation of
// these open sections is unverified, see angleUserSection / channelUserSection).
#include "MidasSectionLibrary.hpp"
#include <cmath>
#include <string>
#include <vector>

namespace kopruq::io::midas {

using SectionOutline = std::vector<sections::Contour>;

namespace detail_outline {
inline constexpr int kCircleSegments = 48;
inline sections::Contour rect(Real y0, Real y1, Real z0, Real z1) { // counter-clockwise
  return {{y0, z0}, {y1, z0}, {y1, z1}, {y0, z1}};
}
inline sections::Contour reversed(sections::Contour c) { std::reverse(c.begin(), c.end()); return c; }
} // namespace detail_outline

// Outline of a MIDAS DB/User section (SHAPE, vSIZE) with the top at z = 0. Throws for shapes the
// section library does not support (the importer has already rejected those).
inline SectionOutline dbUserSectionOutline(const std::string& shape, const std::vector<Real>& v) {
  using namespace detail_outline;
  using sections::Contour;
  const Real pi = std::acos(Real(-1));
  auto need = [&](std::size_t n) { if (v.size() < n) throw std::invalid_argument("section outline: too few vSIZE values for " + shape); };
  if (shape == "SB") { need(2); return {rect(-v[1] / 2, v[1] / 2, -v[0], 0)}; }
  if (shape == "SR") {
    need(1);
    const Real r = v[0] / 2;
    Contour c;
    for (int i = 0; i < kCircleSegments; ++i) { const Real a = 2 * pi * i / kCircleSegments; c.emplace_back(r * std::cos(a), -r + r * std::sin(a)); }
    return {c};
  }
  if (shape == "B") {
    need(4);
    const Real H = v[0], B = v[1], tw = v[2], tf = v[3];
    return {rect(-B / 2, B / 2, -H, 0), reversed(rect(-B / 2 + tw, B / 2 - tw, -H + tf, -tf))};
  }
  if (shape == "T") {
    need(4);
    const Real H = v[0], B = v[1], tw = v[2], tf = v[3];
    return {{{-tw / 2, -H}, {tw / 2, -H}, {tw / 2, -tf}, {B / 2, -tf}, {B / 2, 0}, {-B / 2, 0}, {-B / 2, -tf}, {-tw / 2, -tf}}};
  }
  if (shape == "H" || shape == "C") {
    need(6);
    Real B2 = v[4], tf2 = v[5];
    if (B2 == 0 && tf2 == 0) { B2 = v[1]; tf2 = v[3]; }
    const Real H = v[0], B1 = v[1], tw = v[2], tf1 = v[3];
    if (shape == "H")
      return {{{-B2 / 2, -H}, {B2 / 2, -H}, {B2 / 2, -H + tf2}, {tw / 2, -H + tf2}, {tw / 2, -tf1}, {B1 / 2, -tf1},
               {B1 / 2, 0}, {-B1 / 2, 0}, {-B1 / 2, -tf1}, {-tw / 2, -tf1}, {-tw / 2, -H + tf2}, {-B2 / 2, -H + tf2}}};
    // channel: web outer face at y = 0, flanges towards +y (measured from the web's outer face)
    return {{{0, -H}, {B2, -H}, {B2, -H + tf2}, {tw, -H + tf2}, {tw, -tf1}, {B1, -tf1}, {B1, 0}, {0, 0}}};
  }
  if (shape == "L") {
    need(4);
    const Real H = v[0], B = v[1], tw = v[2], tf = v[3];
    return {{{0, -H}, {B, -H}, {B, -H + tf}, {tw, -H + tf}, {tw, 0}, {0, 0}}};
  }
  if (shape == "STRK") {
    need(2);
    const Real H = v[0], B = v[1], R = H / 2, half = (B - H) / 2;
    Contour c;
    const int n = kCircleSegments / 2;
    for (int i = 0; i <= n; ++i) { const Real a = -pi / 2 + pi * i / n; c.emplace_back(half + R * std::cos(a), -R + R * std::sin(a)); }  // right half circle
    for (int i = 0; i <= n; ++i) { const Real a = pi / 2 + pi * i / n; c.emplace_back(-half + R * std::cos(a), -R + R * std::sin(a)); } // left half circle
    return {c};
  }
  if (shape == "SOCT") {
    need(4);
    const Real H = v[0], B = v[1], a = v[2], b = v[3];
    return {{{-B / 2 + a, -H}, {B / 2 - a, -H}, {B / 2, -H + b}, {B / 2, -b}, {B / 2 - a, 0}, {-B / 2 + a, 0}, {-B / 2, -b}, {-B / 2, -H + b}}};
  }
  throw std::invalid_argument("section outline: unsupported shape '" + shape + "'");
}

// Shifts an outline so that its centroid (exact polygon integration, holes included) is at the origin.
inline SectionOutline centroidalOutline(SectionOutline outline) {
  const auto g = sections::computePolygonSection(outline);
  for (auto& c : outline) for (auto& p : c) { p.x() -= g.Cy; p.y() -= g.Cz; }
  return outline;
}

} // namespace kopruq::io::midas
