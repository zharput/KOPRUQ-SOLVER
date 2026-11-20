// 2026-11-20 -- 2D FE cross-section analysis (kopruq/sections/SectionFE.hpp): torsion constant J
// (warping function) and shear areas (Pilkey shear functions). Dedicated new file.
//
// Every expected value is independent of the production code:
//  * rectangle 2 x 1, J: Saint-Venant series a b^3/3 (1 - 192/pi^5 b/a sum tanh(n pi a / 2b)/n^5),
//    n odd, = 0.4573633545224669 (Python);
//  * rectangle, nu = 0: shear area exactly 5/6 A (energy-consistent Timoshenko value);
//  * rectangle 2 x 1, nu = 0.2: an independent Python implementation of the same Pilkey equations
//    (scipy Delaunay mesh) gives As/A = 0.83338 (shear along the 2-long side) and 0.80796 (along
//    the 1-long side) at h = 0.04;
//  * solid circle radius 1 (360-gon): J = pi/2, As = 6/7 A (Cowper, nu = 0);
//  * circular tube R = 1, r = 0.7 (256-/180-gons; multiply connected): J = pi (R^4 - r^4) / 2 =
//    1.1936481, As = A 6(1+m^2)^2 / (7(1+m^2)^2 + 20 m^2), m = r/R (Cowper, nu = 0) = 0.842199.
// Tolerances reflect the mesh (FE J converges from above) and the polygonal circle approximation.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/sections/SectionFE.hpp"

using namespace kopruq::sections;

namespace {
constexpr double kPiT = 3.14159265358979323846;
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
Contour circle(double r, int n) {
  Contour c;
  for (int i = 0; i < n; ++i) c.emplace_back(r * std::cos(2 * kPiT * i / n), r * std::sin(2 * kPiT * i / n));
  return c;
}
const Contour kRect{{-1, -0.5}, {1, -0.5}, {1, 0.5}, {-1, 0.5}};
}

TEST(SectionFE, RectangleTorsionMatchesSaintVenantSeriesAndConvergesFromAbove) {
  const auto coarse = analyzeSectionFE({kRect}, 0.05, 0.0);
  const auto fine = analyzeSectionFE({kRect}, 0.025, 0.0);
  rel(fine.area, 2.0, "meshed area", 1e-12);
  rel(fine.J, 0.4573633545224669, "J vs series", 1e-3);
  EXPECT_GT(coarse.J, fine.J);  // conforming linear FE: energy bound, J decreases with refinement
  EXPECT_GT(fine.J, 0.4573633545224669);
}

TEST(SectionFE, RectangleShearAreasAreFiveSixthsForZeroPoisson) {
  const auto r = analyzeSectionFE({kRect}, 0.025, 0.0);
  rel(r.Asy, 5.0 / 6.0 * 2.0, "Asy = 5/6 A", 2e-3);
  rel(r.Asz, 5.0 / 6.0 * 2.0, "Asz = 5/6 A", 2e-3);
  rel(r.Iyy, 2.0 * 1.0 / 12.0, "Iy = b h^3/12 from the mesh", 1e-9);
  rel(r.Izz, 1.0 * 8.0 / 12.0, "Iz from the mesh", 1e-9);
}

TEST(SectionFE, RectanglePoissonDependenceMatchesIndependentPython) {
  const auto r = analyzeSectionFE({kRect}, 0.025, 0.2);
  rel(r.Asy / r.area, 0.83338, "along the long side (independent Python FE)", 2e-3);
  rel(r.Asz / r.area, 0.80796, "along the short side (independent Python FE)", 3e-3);
}

TEST(SectionFE, SolidCircleTorsionAndShear) {
  const auto r = analyzeSectionFE({circle(1.0, 360)}, 0.03, 0.0);
  rel(r.J, kPiT / 2, "J = pi r^4 / 2", 2e-3);
  rel(r.Asy, 6.0 / 7.0 * r.area, "Asy = 6/7 A", 3e-3);
  rel(r.Asz, 6.0 / 7.0 * r.area, "Asz = 6/7 A", 3e-3);
}

TEST(SectionFE, CircularTubeMultiplyConnectedTorsionAndShear) {
  const auto r = analyzeSectionFE({circle(1.0, 256), circle(0.7, 180)}, 0.025, 0.0);
  rel(r.area, kPiT * (1.0 - 0.49), "area (polygons)", 2e-4);
  rel(r.J, kPiT * (1.0 - 0.7 * 0.7 * 0.7 * 0.7) / 2, "J = pi (R^4 - r^4) / 2", 2e-3);
  rel(r.Asy, 0.842199, "Asy (Cowper, nu = 0)", 3e-3);
  rel(r.Asz, 0.842199, "Asz (Cowper, nu = 0)", 3e-3);
}

TEST(SectionFE, HoleWindingDoesNotMatter) {
  Contour hole = circle(0.7, 180);
  Contour reversed(hole.rbegin(), hole.rend());
  const auto a = analyzeSectionFE({circle(1.0, 256), hole}, 0.05, 0.0);
  const auto b = analyzeSectionFE({circle(1.0, 256), reversed}, 0.05, 0.0);
  rel(b.area, a.area, "area", 1e-12);
  rel(b.J, a.J, "J (same point set; co-circular ties may triangulate differently)", 1e-6);
}

TEST(SectionFE, RejectsInvalidInput) {
  EXPECT_THROW(analyzeSectionFE({}, 0.1), std::invalid_argument);
  EXPECT_THROW(analyzeSectionFE({kRect}, 0.0), std::invalid_argument);
  EXPECT_THROW(analyzeSectionFE({Contour{{0, 0}, {1, 0}}}, 0.1), std::invalid_argument);
}
