// Phase 7G.1 addendum (2026-11-19) -- "STRK" (Solid Track) and "SOCT" (Solid Octagon,
// actually a rectangle with 4 independently-dimensioned chamfered corners) DBUSER section
// shapes (MidasSectionLibrary.hpp: solidTrack, solidOctagon). Dedicated new file, per
// CLAUDE.md's "never overwrite or truncate a test file to add new tests" rule --
// midas_section_library_tests.cpp is untouched.
//
// Every expected value below is hand-derived from classical mechanics of materials (composite-
// shape decomposition via the parallel-axis theorem), independently of the production
// solidTrack/solidOctagon implementation, using model 21's own real section dimensions
// (verification/models/21 Combo, sections 4 "SEC4"/STRK and 5 "SEC5"/SOCT) where convenient.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-3) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

// Degenerate case B=H (no straight midsection, Ls=0): a track reduces to exactly a circle of
// diameter H. Area and IH/IB are each other's EXACT closed form (both derived from the same
// composite decomposition's semicircle-pair terms, which is just the full circle at Ls=0), so
// this is an exact check, not an approximate one.
TEST(MidasSectionLibraryStrkSoct, SolidTrackDegenerateCaseMatchesSolidCircleExactly) {
  auto t = solidTrack(0.9, 0.9);
  auto c = solidCircle(0.9);
  rel(t.area, c.area, "area", 1e-9);
  rel(t.IH, c.IH, "IH", 1e-9);
  rel(t.IB, c.IB, "IB", 1e-9);
  // J is each shape's own numerical FDM solve (solidCircle itself is exact pi*D^4/32, solidTrack
  // is the general FDM solver evaluated at a near-degenerate boundary) -- not expected to be
  // bit-identical, but should be close: curved-boundary staircase aliasing, not a formulation
  // difference (see solidTrack's/torsionConstantFEMStadium's own header comment).
  EXPECT_NEAR(t.J, c.J, 0.01 * c.J);
}

// Model 21's own "SEC4"/STRK dimensions (H=0.9, B=2.0 m, confirmed directly from the user's
// MIDAS NX "Section Data" dialog screenshot, 2026-11-19): independently hand-derived via
// composite decomposition (central Ls=B-H=1.1m x H=0.9m rectangle + two semicircular end caps
// of radius R=H/2=0.45m, i.e. together one full circle) -- see solidTrack's own header comment
// for the full derivation; reproduced here arithmetically.
TEST(MidasSectionLibraryStrkSoct, SolidTrackMatchesHandComputation_Model21Sec4) {
  const double H = 0.9, B = 2.0, R = H / 2.0, Ls = B - H;
  auto p = solidTrack(H, B);
  const double expectedArea = Ls * H + kPi * R * R;
  rel(p.area, expectedArea, "area", 1e-9);
  const double expectedIH = kPi * std::pow(R, 4) / 4.0 + (2.0 * Ls * std::pow(R, 3)) / 3.0;
  rel(p.IH, expectedIH, "IH", 1e-9);
  const double expectedIB = kPi * std::pow(R, 4) / 4.0 + (4.0 * Ls * std::pow(R, 3)) / 3.0 +
                             (kPi * Ls * Ls * R * R) / 4.0 + (R * std::pow(Ls, 3)) / 6.0;
  rel(p.IB, expectedIB, "IB", 1e-9);
  EXPECT_GT(p.J, 0.0);
  EXPECT_GT(p.AsH, 0.0);
  EXPECT_LT(p.AsH, p.area); // a shear area must never exceed the gross area
  EXPECT_GT(p.AsB, 0.0);
  EXPECT_LT(p.AsB, p.area);
}

// Degenerate case a=b=0 (no chamfer): a "Solid Octagon" reduces to exactly a plain rectangle --
// exact check (same composite decomposition's 4-triangle terms vanish identically at a=b=0,
// including the torsion dispatch, which falls back to solidRectangleTorsion exactly in that
// case -- see solidOctagon's own header comment).
TEST(MidasSectionLibraryStrkSoct, SolidOctagonDegenerateCaseMatchesSolidRectangleExactly) {
  auto o = solidOctagon(1.0, 2.0, 0.0, 0.0);
  auto r = solidRectangle(1.0, 2.0);
  rel(o.area, r.area, "area", 1e-9);
  rel(o.IH, r.IH, "IH", 1e-9);
  rel(o.IB, r.IB, "IB", 1e-9);
  rel(o.J, r.J, "J", 1e-9); // a=b=0 dispatches to the exact same closed-form rectangle formula
  rel(o.AsH, r.AsH, "AsH", 1e-9);
  rel(o.AsB, r.AsB, "AsB", 1e-9);
}

// Model 21's own "SEC5"/SOCT dimensions (H=1.0, B=2.0, a=0.3, b=0.2 m, confirmed directly from
// the user's MIDAS NX "Section Data" dialog screenshot, 2026-11-19): independently hand-derived
// via full-rectangle-minus-4-corner-triangles (standard right-triangle centroid/inertia results
// a^3*b/12 and a*b^3/12 about the leg axes, plus the parallel-axis theorem) -- see
// solidOctagon's own header comment for the full derivation; reproduced here arithmetically.
TEST(MidasSectionLibraryStrkSoct, SolidOctagonMatchesHandComputation_Model21Sec5) {
  const double H = 1.0, B = 2.0, a = 0.3, b = 0.2;
  auto p = solidOctagon(H, B, a, b);
  const double expectedArea = H * B - 2.0 * a * b;
  rel(p.area, expectedArea, "area", 1e-9);
  const double expectedIB = H * std::pow(B, 3) / 12.0 -
                             (B * B * a * b / 2.0 - (2.0 * B * a * a * b) / 3.0 + std::pow(a, 3) * b / 3.0);
  rel(p.IB, expectedIB, "IB", 1e-9);
  const double expectedIH = B * std::pow(H, 3) / 12.0 -
                             (H * H * a * b / 2.0 - (2.0 * H * a * b * b) / 3.0 + a * std::pow(b, 3) / 3.0);
  rel(p.IH, expectedIH, "IH", 1e-9);
  EXPECT_GT(p.J, 0.0);
  EXPECT_LT(p.J, solidRectangle(H, B).J); // chamfering material away can only reduce torsional stiffness
  EXPECT_GT(p.AsH, 0.0);
  EXPECT_LT(p.AsH, p.area);
  EXPECT_GT(p.AsB, 0.0);
  EXPECT_LT(p.AsB, p.area);
}

TEST(MidasSectionLibraryStrkSoct, RejectsInvalidDimensions) {
  EXPECT_THROW(solidTrack(0.0, 2.0), std::invalid_argument);   // H must be positive
  EXPECT_THROW(solidTrack(2.0, 1.0), std::invalid_argument);   // B must be >= H
  EXPECT_THROW(solidOctagon(1.0, 2.0, 1.1, 0.2), std::invalid_argument); // 2a > B
  EXPECT_THROW(solidOctagon(1.0, 2.0, 0.3, 0.6), std::invalid_argument); // 2b > H
}

TEST(MidasSectionLibraryStrkSoct, DispatchMatchesDirectCalls) {
  auto viaDispatch = resolveDbUserSection("STRK", {0.9, 2.0});
  auto direct = solidTrack(0.9, 2.0);
  rel(viaDispatch.area, direct.area, "STRK dispatch area", 1e-9);
  rel(viaDispatch.IH, direct.IH, "STRK dispatch IH", 1e-9);

  auto viaDispatch2 = resolveDbUserSection("SOCT", {1.0, 2.0, 0.3, 0.2});
  auto direct2 = solidOctagon(1.0, 2.0, 0.3, 0.2);
  rel(viaDispatch2.area, direct2.area, "SOCT dispatch area", 1e-9);
  rel(viaDispatch2.IH, direct2.IH, "SOCT dispatch IH", 1e-9);
}
