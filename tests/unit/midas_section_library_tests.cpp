// Phase 7B — MIDAS DBUSER section shape resolution (MidasSectionLibrary.hpp).
// Every expected value below is hand-computed from classical formulas
// (not by calling the production functions), using the real dimensions
// found in xmidas/deneme1.json's SECT records where convenient.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-3) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(MidasSectionLibrary, SolidRectangleMatchesHandComputation_SuperDeck) {
  // SECT 1 "SUPER": vSIZE=[1.0,12.0] -- a 1 m deep, 12 m wide slab.
  auto p = solidRectangle(1.0, 12.0);
  rel(p.area, 12.0, "area");
  rel(p.IH, 1.0, "IH = B*H^3/12 = 12*1/12");
  rel(p.IB, 144.0, "IB = H*B^3/12 = 1*1728/12");
  // J = a*b^3*(1/3 - 0.21*r*(1-r^4/12)), a=12,b=1,r=1/12
  const double r = 1.0/12.0;
  const double expectedJ = 12.0*1.0*(1.0/3.0 - 0.21*r*(1.0 - std::pow(r,4)/12.0));
  rel(p.J, expectedJ, "torsion constant");
}

TEST(MidasSectionLibrary, SolidRectangleSquareTorsionIsNearTabulatedCoefficient) {
  // Solid square, side a=2: tabulated torsion constant J ~ 0.1406*a^4 (classical result).
  auto p = solidRectangle(2.0, 2.0);
  rel(p.area, 4.0, "area");
  rel(p.IH, 4.0*4.0/12.0, "IH", 1e-9);
  rel(p.IB, 4.0*4.0/12.0, "IB", 1e-9);
  const double tabulated = 0.1406 * std::pow(2.0, 4);
  EXPECT_NEAR(p.J, tabulated, 0.05 * tabulated); // approximate formula vs. tabulated exact value, 5% band
}

TEST(MidasSectionLibrary, SolidCircleMatchesHandComputation_Pier3) {
  // SECT 4 "PIER-3": vSIZE=[1.2] -- D=1.2 m.
  auto p = solidCircle(1.2);
  const double D = 1.2, r = D/2.0;
  rel(p.area, kPi*r*r, "area");
  rel(p.IH, kPi*std::pow(D,4)/64.0, "IH");
  rel(p.IB, kPi*std::pow(D,4)/64.0, "IB");
  rel(p.J, kPi*std::pow(D,4)/32.0, "J");
  rel(p.J, 2.0*p.IH, "J == 2*I for a circle", 1e-9);
}

TEST(MidasSectionLibrary, HollowBoxMatchesHandComputation_Pier2) {
  // SECT 3 "PIER-2": vSIZE=[2.0,5.0,0.5,0.5] -- H=2,B=5,tw=tf=0.5.
  auto p = hollowBox(2.0, 5.0, 0.5, 0.5);
  rel(p.area, 6.0, "area = 2*5 - 1*4");
  rel(p.IH, 3.0, "IH = (5*8 - 4*1)/12");
  rel(p.IB, 15.5, "IB = (2*125 - 1*64)/12");
  rel(p.J, 7.59375, "Bredt torsion constant", 1e-6);
  // AsH/AsB (2026-11-20, user decision: match MIDAS NX): MIDAS's gross-wall-area convention,
  // AsH = 2*tw*H = 2.0, AsB = 2*tf*B = 5.0 (confirmed against MIDAS data on models 18 and 21);
  // supersedes the 2026-11-19 Zhuravskii expectation 2.188449 / 4.476007.
  rel(p.AsH, 2.0, "AsH = 2*tw*H");
  rel(p.AsB, 5.0, "AsB = 2*tf*B");
}

TEST(MidasSectionLibrary, TSectionMatchesHandComputation_Baslik) {
  // SECT 3 "BASLIK" (verification/models/17 and /18, a pier cap): vSIZE=[2.0,2.0,1.0,1.0] --
  // H=2, B=2, tw=1, tf=1. Hand-computed via composite-section parallel-axis theorem (flange +
  // web, centroid measured down from the top fiber).
  auto p = tSection(2.0, 2.0, 1.0, 1.0);
  rel(p.area, 3.0, "area = 2*1 + 1*1");
  rel(p.IH, 0.9166666666666666, "IH (composite, parallel axis)", 1e-9);
  rel(p.IB, 0.75, "IB = 1*8/12 + 1*1/12", 1e-9);
  // J (2026-11-19): deliberately the thin-strip-sum approximation, NOT torsionConstantFEM's more
  // exact membrane-analogy solve -- tried the FEM solve first, but it made model 17's real-data
  // match WORSE (see MidasSectionLibrary.hpp's own comment on tSection() for the evidence),
  // strong evidence MIDAS's own DBUSER section uses this simpler convention internally. Matching
  // MIDAS's own convention, not the more "physically rigorous" answer, is what this import needs.
  // J, AsH, AsB (2026-11-20, user decision: match MIDAS NX): MIDAS's own DBUSER T convention,
  // back-calculated from MIDAS's results for exactly this section (models 18 and 22):
  // J = Roark's T-section formula = 1.0528515625 (independent Python; MIDAS 1.052851615),
  // AsH = tw*H = 2.0, AsB = 5/6*B*tf = 5/3. Supersedes the 2026-11-19 expectations (strip-sum
  // J = 1.0; Zhuravskii AsH = 605/258, AsB = 2.7480998869943494), which MIDAS does not use.
  rel(p.J, 1.0528515625, "J = Roark T-section formula (MIDAS convention)", 1e-9);
  rel(p.AsH, 2.0, "AsH = tw*H (MIDAS convention)", 1e-9);
  rel(p.AsB, 5.0 / 3.0, "AsB = 5/6*B*tf (MIDAS convention)", 1e-9);
}

TEST(MidasSectionLibrary, TimoshenkoShearAreaMatchesClosedFormRectangle) {
  // The generic Zhuravskii/Timoshenko quadrature (detail::timoshenkoShearArea), applied to a
  // single rectangular strip, must reproduce the EXACT classical closed-form result k=5/6 for a
  // solid rectangle (Timoshenko & Goodier, already used directly in solidRectangle() above) --
  // derived independently by hand: As = I^2 / integral(Q(y)^2/b dy), with Q(y)=(B/2)(H^2/4-y^2)
  // for a rectangle of depth H, width B, centered at y=0; the integral evaluates in closed form
  // to B*H^5/120, giving As = (B*H^3/12)^2 / (B*H^5/120) = (5/6)*B*H exactly.
  const double H = 2.0, B = 3.0;
  const double I = B * H * H * H / 12;
  const double As = detail::timoshenkoShearArea({{0, H, B}}, I);
  rel(As, (5.0 / 6.0) * B * H, "As == (5/6)*A for a solid rectangle, exact closed form", 1e-6);
}

TEST(MidasSectionLibrary, TorsionConstantFEMMatchesTabulatedSquareCoefficient) {
  // detail::torsionConstantFEM applied to a plain square must reproduce the classical TABULATED
  // (not merely approximate) coefficient J=0.1406*a^4 (same target as
  // SolidRectangleSquareTorsionIsNearTabulatedCoefficient above, which checks the OTHER,
  // closed-form approximate formula against this same tabulated value).
  const double a = 2.0;
  const double J = detail::torsionConstantFEM({{0, a, 0, a}});
  const double tabulated = 0.1406 * a * a * a * a;
  EXPECT_NEAR(J, tabulated, 0.005 * tabulated); // 0.5% band -- FDM discretization vs. the exact series solution
}

TEST(MidasSectionLibrary, TorsionConstantFEMMatchesClosedFormRectangle) {
  // For a plain (non-square) rectangle, detail::torsionConstantFEM must agree closely with this
  // file's own existing closed-form solidRectangleTorsion() approximation -- both are independent
  // approximations to the same exact answer, so close agreement cross-validates both.
  const double H = 2.0, B = 3.0;
  const double J = detail::torsionConstantFEM({{0, B, 0, H}});
  const double closedForm = detail::solidRectangleTorsion(B, H);
  rel(J, closedForm, "FEM vs. closed-form rectangle torsion constant", 2e-3);
}

TEST(MidasSectionLibrary, RejectsUnsupportedShapeAndInvalidDimensions) {
  EXPECT_THROW(resolveDbUserSection("H", {1.0, 1.0}), std::invalid_argument);
  EXPECT_THROW(solidRectangle(-1.0, 2.0), std::invalid_argument);
  EXPECT_THROW(solidCircle(0.0), std::invalid_argument);
  EXPECT_THROW(hollowBox(1.0, 1.0, 0.6, 0.1), std::invalid_argument); // 2*tw=1.2 >= B=1.0, no interior
  EXPECT_THROW(tSection(1.0, 1.0, 0.1, 1.0), std::invalid_argument); // tf >= H, no web height left
  EXPECT_THROW(tSection(2.0, 1.0, 1.5, 0.5), std::invalid_argument); // tw > B, impossible web
}

TEST(MidasSectionLibrary, DispatchMatchesDirectCalls) {
  auto viaDispatch = resolveDbUserSection("SB", {1.0, 12.0});
  auto direct = solidRectangle(1.0, 12.0);
  rel(viaDispatch.area, direct.area, "area");
  rel(viaDispatch.IH, direct.IH, "IH");
  rel(viaDispatch.IB, direct.IB, "IB");
  rel(viaDispatch.J, direct.J, "J");
}
