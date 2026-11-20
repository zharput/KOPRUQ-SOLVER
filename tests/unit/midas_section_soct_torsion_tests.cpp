// 2026-11-20 -- MIDAS NX DBUSER "SOCT" (Solid Octagon) torsion-constant convention, adopted by user
// decision (match MIDAS). Dedicated new file (CLAUDE.md: never overwrite or truncate an existing
// test file to add tests).
//
//   J = Roark solid-rectangle formula on the reduced rectangle (B - a/4) x (H - b/4),
//   Roark: J = A_long * b^3 * (1/3 - 0.21 (b/A_long) (1 - (b/A_long)^4 / 12)),  b = short side.
//
// Expected values are computed independently (Python, outside this code base) from the formula
// above and written as literals. Each one is ALSO what MIDAS itself produces for that exact
// section: J back-calculated per element (J = T*L/(G*dtheta)) from MIDAS's own model-21
// displacement and beam-force exports, 6 significant digits, consistent with MIDAS's own
// Section_prop export (4 digits). The geometries are the controlled series the user built in
// MIDAS for this purpose (chamfered, a=b~0, a only, b only); MIDAS itself was given 1e-5 for a
// "zero" leg, reproduced here.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
// MIDAS's own per-element back-calculated J (6 significant digits): the formula must reproduce it
// to within the 6th digit, i.e. a relative 5e-6.
constexpr double kMidasSixDigit = 5e-6;
}

// Model 21 SEC5 [H,B,a,b] = [1.0, 2.0, 0.3, 0.2]. MIDAS: 0.379948 (back-calc), 0.3799 (Section_prop).
TEST(MidasSoctTorsion, Model21Sec5Chamfered) {
  const double J = solidOctagon(1.0, 2.0, 0.3, 0.2).J;
  rel(J, 0.37994812703428404, "formula");
  rel(J, 0.379948, "MIDAS back-calc", kMidasSixDigit);
}

// Model 21 SEC7 (current revision) [1.3, 2.5, 1.0, 0.5]. MIDAS: 0.818870 / 0.8189.
TEST(MidasSoctTorsion, Model21Sec7LargeChamfer) {
  const double J = solidOctagon(1.3, 2.5, 1.0, 0.5).J;
  rel(J, 0.8188703586443616, "formula");
  rel(J, 0.818870, "MIDAS back-calc", kMidasSixDigit);
}

// Model 21 SEC7 (previous revision) [1.3, 2.5, 0.4, 0.25]. MIDAS: 1.026500 (back-calc).
TEST(MidasSoctTorsion, Model21Sec7PreviousRevision) {
  const double J = solidOctagon(1.3, 2.5, 0.4, 0.25).J;
  rel(J, 1.0265001628185, "formula");
  rel(J, 1.026500, "MIDAS back-calc", kMidasSixDigit);
}

// Model 21 SEC8 [1.3, 2.5, 1e-5, 1e-5] (no chamfer). MIDAS: 1.234699 / 1.2347.
TEST(MidasSoctTorsion, Model21Sec8NoChamfer) {
  const double J = solidOctagon(1.3, 2.5, 1e-5, 1e-5).J;
  rel(J, 1.2346989857430601, "formula");
  rel(J, 1.234699, "MIDAS back-calc", kMidasSixDigit);
}

// Model 21 SEC9 [1.3, 2.5, 0.45, 1e-5] (horizontal leg only, zero cut area). MIDAS: 1.153053 / 1.1531.
// MIDAS's convention lowers J although the section is geometrically still the rectangle.
TEST(MidasSoctTorsion, Model21Sec9HorizontalLegOnly) {
  const auto p = solidOctagon(1.3, 2.5, 0.45, 1e-5);
  rel(p.J, 1.153052799497047, "formula");
  rel(p.J, 1.153053, "MIDAS back-calc", kMidasSixDigit);
  rel(p.area, 3.249991, "area is still the rectangle's (minus 2ab)");
}

// Model 21 SEC10 [1.3, 2.5, 1e-5, 0.5] (vertical leg only). MIDAS: 0.953202 / 0.9532.
TEST(MidasSoctTorsion, Model21Sec10VerticalLegOnly) {
  const double J = solidOctagon(1.3, 2.5, 1e-5, 0.5).J;
  rel(J, 0.9532020264584963, "formula");
  rel(J, 0.953202, "MIDAS back-calc", kMidasSixDigit);
}

// a = b = 0 reduces exactly to the plain rectangle (Roark on B x H): independent value for 2.0 x 1.0.
TEST(MidasSoctTorsion, ZeroChamferIsThePlainRectangle) {
  rel(solidOctagon(1.0, 2.0, 0.0, 0.0).J, 0.45776041666666667, "Roark 2.0 x 1.0");
  rel(solidOctagon(1.0, 2.0, 0.0, 0.0).J, solidRectangle(1.0, 2.0).J, "same as solidRectangle");
}

// The exact St. Venant J of the chamfered rectangle (Prandtl membrane FDM) stays available as a
// separate function. Independent Python FDM (h = H/300): 0.437099 for SEC5; the library's coarser
// grid is within ~1%. MIDAS's convention is ~13% lower here (but ~28% HIGHER for the large chamfer
// [1.3,2.5,1.0,0.5]: exact 0.637362 vs 0.818870 -- the convention is not a bound either way).
TEST(MidasSoctTorsion, ExactStVenantStillAvailableAndAboveMidasConvention) {
  const double exact = detail::torsionConstantFEMChamferedRect(1.0, 2.0, 0.3, 0.2);
  rel(exact, 0.437099, "SEC5 exact J (independent FDM)", 1.5e-2);
  EXPECT_GT(exact, solidOctagon(1.0, 2.0, 0.3, 0.2).J);
}

TEST(MidasSoctTorsion, DispatchUsesTheConvention) {
  rel(resolveDbUserSection("SOCT", {1.3, 2.5, 1.0, 0.5}).J, 0.8188703586443616, "SOCT J via dispatch");
}
