// 2026-11-20 -- MIDAS NX DBUSER section-property conventions adopted by user decision (match
// MIDAS): box (B) shear areas, STRK torsion constant and shear areas, SOCT shear areas.
// Dedicated new file (CLAUDE.md: never overwrite or truncate an existing test file to add tests).
//
// Expected values are computed independently (Python, outside this code base) from the closed
// forms below and written as literals. The same numbers are ALSO what MIDAS itself reports for
// these exact sections -- its Section_prop.xlsx export of verification model 21 (4-5 digits) and
// the 6-digit values back-calculated from MIDAS's own displacement/beam-force results -- which is
// the evidence these conventions are MIDAS's own (see docs/DEVELOPMENT_STATUS.md, 2026-11-20).
//   STRK (H, B; R=H/2, Ls=B-H): J   = Roark rectangle on the equal-area rectangle (A/H x H)
//                               AsH = (5/6)*Ls*H + 0.9*pi*R^2,  AsB = Ls*H + 0.9*pi*R^2
//   SOCT:                        AsH = AsB = (5/6)*A  (J: exact St. Venant, not MIDAS's)
//   Box  (H, B, tw, tf):         AsH = 2*tw*H,        AsB = 2*tf*B
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

// Model 21 SEC4: H=0.9, B=2.0. MIDAS: J 0.301992 (back-calc) / 0.302 (Section_prop),
// Asy 1.5626, Asz 1.3976 (Section_prop).
TEST(MidasSectionConventions, StrkModel21Sec4MatchesMidasConvention) {
  auto p = solidTrack(0.9, 2.0);
  rel(p.area, 1.6261725123519333, "A");
  rel(p.J, 0.3019923541805103, "J (Roark on equal-area rectangle)");
  rel(p.AsH, 1.3975552611167399, "AsH = 5/6*Ls*H + 0.9*pi*R^2");
  rel(p.AsB, 1.56255526111674, "AsB = Ls*H + 0.9*pi*R^2");
}

// Model 21 SEC6: H=1.2, B=3.0 -- the second, independent geometry. MIDAS: J 1.145541
// (back-calc) / 1.1455, Asy 3.1779, Asz 2.8179 (Section_prop).
TEST(MidasSectionConventions, StrkModel21Sec6MatchesMidasConvention) {
  auto p = solidTrack(1.2, 3.0);
  rel(p.area, 3.2909733552923255, "A");
  rel(p.J, 1.1455414062315565, "J (Roark on equal-area rectangle)");
  rel(p.AsH, 2.8178760197630925, "AsH");
  rel(p.AsB, 3.177876019763093, "AsB");
}

// The exact St. Venant J of the stadium (Prandtl membrane FDM) is kept as a separate, still-
// verified function: independent Python finite-difference solve (h = H/300) gives 0.316636 for
// H=0.9, B=2.0; the library's coarser grid is within ~0.5%. MIDAS's convention is ~4.6% lower.
TEST(MidasSectionConventions, ExactStadiumTorsionStillAvailableAndAboveMidasConvention) {
  const double exactJ = detail::torsionConstantFEMStadium(1.1, 0.9);
  rel(exactJ, 0.316636, "exact stadium J (independent FDM)", 1e-2);
  EXPECT_GT(exactJ, solidTrack(0.9, 2.0).J);
}

// Model 21 SEC5 (1.0, 2.0, 0.3, 0.2) and SEC7 (1.3, 2.5, 0.4, 0.25): AsH = AsB = 5/6*A.
// MIDAS Section_prop: 1.5667 / 2.5417 for both directions.
TEST(MidasSectionConventions, SoctShearAreasAreFiveSixthsOfArea) {
  auto p5 = solidOctagon(1.0, 2.0, 0.3, 0.2);
  rel(p5.AsH, 1.88 * 5.0 / 6.0, "SEC5 AsH");
  rel(p5.AsB, 1.88 * 5.0 / 6.0, "SEC5 AsB");
  auto p7 = solidOctagon(1.3, 2.5, 0.4, 0.25);
  rel(p7.AsH, 3.05 * 5.0 / 6.0, "SEC7 AsH");
  rel(p7.AsB, 3.05 * 5.0 / 6.0, "SEC7 AsB");
}

// SOCT J stays the exact St. Venant value (MIDAS's own SOCT J, 0.379948 / 1.026500, matched no
// closed form and is deliberately not reproduced). Independent Python FDM (h=H/300): 0.437099 and
// 1.176001; the library's coarser grid is within ~1%.
TEST(MidasSectionConventions, SoctTorsionIsExactStVenantNotMidas) {
  rel(solidOctagon(1.0, 2.0, 0.3, 0.2).J, 0.437099, "SEC5 exact J (independent FDM)", 1.5e-2);
  rel(solidOctagon(1.3, 2.5, 0.4, 0.25).J, 1.176001, "SEC7 exact J (independent FDM)", 1.5e-2);
}

// Model 21 SEC3 box B[1.5,1.0,0.1,0.2]: MIDAS Section_prop Asy 0.4, Asz 0.3.
TEST(MidasSectionConventions, BoxModel21Sec3MatchesMidasGrossWallAreas) {
  auto p = hollowBox(1.5, 1.0, 0.1, 0.2);
  rel(p.AsH, 2 * 0.1 * 1.5, "AsH = 2*tw*H = 0.3");
  rel(p.AsB, 2 * 0.2 * 1.0, "AsB = 2*tf*B = 0.4");
  rel(p.J, 0.15644571428571428, "Bredt J unchanged (MIDAS back-calc 0.156446)", 1e-6);
}

TEST(MidasSectionConventions, DispatchUsesTheSameConventions) {
  auto s = resolveDbUserSection("STRK", {1.2, 3.0});
  rel(s.J, 1.1455414062315565, "STRK J via dispatch");
  rel(s.AsB, 3.177876019763093, "STRK AsB via dispatch");
  auto o = resolveDbUserSection("SOCT", {1.3, 2.5, 0.4, 0.25});
  rel(o.AsH, 3.05 * 5.0 / 6.0, "SOCT AsH via dispatch");
  auto b = resolveDbUserSection("B", {1.5, 1.0, 0.1, 0.2});
  rel(b.AsB, 0.4, "B AsB via dispatch");
}
