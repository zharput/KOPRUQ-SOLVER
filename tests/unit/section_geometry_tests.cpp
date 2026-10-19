// Phase 7S — general polygonal section property engine (SectionGeometry.hpp).
// Expected values are hand-computed from the classical closed-form
// rectangle formulas (Iz = H*B^3/12, Iy = B*H^3/12 about the centroid),
// not from the polygon-integration code under test.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/sections/SectionGeometry.hpp"

using namespace kopruq::sections;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-9)) << what;
}
}

TEST(SectionGeometry, RectangleMatchesClosedForm) {
  // width (y) = 4, height (z) = 6, centered at the origin.
  Contour outer = {{-2,-3}, {2,-3}, {2,3}, {-2,3}}; // CCW
  auto p = computePolygonSection({outer});
  rel(p.area, 24.0, "area");
  rel(p.Cy, 0.0, "Cy", 1e-12);
  rel(p.Cz, 0.0, "Cz", 1e-12);
  rel(p.Izz, 6.0*4.0*4.0*4.0/12.0, "Izz = H*B^3/12");
  rel(p.Iyy, 4.0*6.0*6.0*6.0/12.0, "Iyy = B*H^3/12");
  rel(p.Iyz, 0.0, "Iyz (symmetric section)", 1e-9);
  rel(p.cy_plus, 2.0, "cy_plus"); rel(p.cy_minus, 2.0, "cy_minus");
  rel(p.cz_plus, 3.0, "cz_plus"); rel(p.cz_minus, 3.0, "cz_minus");
  rel(p.Wz_plus, p.Izz/2.0, "Wz_plus = Izz/cy_plus");
  rel(p.Wy_plus, p.Iyy/3.0, "Wy_plus = Iyy/cz_plus");
}

TEST(SectionGeometry, HollowBoxMatchesDifferenceOfTwoRectangles) {
  // Outer 4x6, centered; hole 2x3, centered -- net = outer minus hole, by hand:
  //   outer Izz=6*64/12=32, Iyy=4*216/12=72; hole Izz=3*8/12=2, Iyy=2*27/12=4.5
  //   net Izz=30, Iyy=67.5, area=24-6=18
  Contour outer = {{-2,-3}, {2,-3}, {2,3}, {-2,3}};       // CCW
  Contour hole  = {{-1,-1.5}, {-1,1.5}, {1,1.5}, {1,-1.5}}; // CW (opposite winding)
  auto p = computePolygonSection({outer, hole});
  rel(p.area, 18.0, "area");
  rel(p.Cy, 0.0, "Cy", 1e-9);
  rel(p.Cz, 0.0, "Cz", 1e-9);
  rel(p.Izz, 30.0, "Izz");
  rel(p.Iyy, 67.5, "Iyy");
  rel(p.I1, 67.5, "principal I1 (symmetric, aligned with y/z)");
  rel(p.I2, 30.0, "principal I2");
}

TEST(SectionGeometry, OffsetRectangleCentroidAndParallelAxisAreConsistent) {
  // Same 4x6 rectangle, shifted so its corner is at the origin: (0,0)-(4,6).
  // Centroid must land at (2,3); Izz/Iyy about the CENTROID must be unchanged.
  Contour outer = {{0,0}, {4,0}, {4,6}, {0,6}};
  auto p = computePolygonSection({outer});
  rel(p.Cy, 2.0, "Cy"); rel(p.Cz, 3.0, "Cz");
  rel(p.Izz, 6.0*4.0*4.0*4.0/12.0, "Izz unaffected by translation");
  rel(p.Iyy, 4.0*6.0*6.0*6.0/12.0, "Iyy unaffected by translation");
}

TEST(SectionGeometry, RejectsDegenerateInput) {
  EXPECT_THROW(computePolygonSection({}), std::invalid_argument);
  EXPECT_THROW(computePolygonSection({{{0,0},{1,0}}}), std::invalid_argument); // only 2 vertices
  // A "hole" identical to the outer boundary (correctly opposite-wound) cancels exactly,
  // leaving zero net area.
  Contour outer = {{-1,-1},{1,-1},{1,1},{-1,1}};       // CCW
  Contour identicalHole = {{-1,-1},{-1,1},{1,1},{1,-1}}; // same shape, CW
  EXPECT_THROW(computePolygonSection({outer, identicalHole}), std::invalid_argument);
}
