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
}

TEST(MidasSectionLibrary, TSectionMatchesHandComputation_Baslik) {
  // SECT 3 "BASLIK" (verification/models/17 and /18, a pier cap): vSIZE=[2.0,2.0,1.0,1.0] --
  // H=2, B=2, tw=1, tf=1. Hand-computed via composite-section parallel-axis theorem (flange +
  // web, centroid measured down from the top fiber).
  auto p = tSection(2.0, 2.0, 1.0, 1.0);
  rel(p.area, 3.0, "area = 2*1 + 1*1");
  rel(p.IH, 0.9166666666666666, "IH (composite, parallel axis)", 1e-9);
  rel(p.IB, 0.75, "IB = 1*8/12 + 1*1/12", 1e-9);
  rel(p.J, 1.0, "J = (2*1 + 1*1)/3", 1e-9);
  rel(p.AsH, 1.0, "AsH = web area = 1*1");
  rel(p.AsB, 5.0/6.0*2.0, "AsB = (5/6)*flange area");
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
