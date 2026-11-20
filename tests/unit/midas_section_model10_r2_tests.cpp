// 2026-11-20 -- verification model 10, second revision (user added H sections and an S355 steel
// material): a second DBUSER "H" geometry and two MIDAS "AISC10(US)" catalog I-shapes.
// Dedicated new file (CLAUDE.md: never overwrite or truncate an existing test file to add tests).
//
// Expected literals are computed independently (Python, outside this code base): the DBUSER H
// values from strip moments about the top fiber; the AISC values from the AISC Shapes Database
// tabulated properties (16th ed., rows W36X247 / HP14X89 as packaged in steelpy 1.1.1), converted
// with 1 in = 0.0254 m exactly. Each is ALSO cross-checked against what MIDAS itself used,
// back-calculated per element from MIDAS's own displacement/beam-force exports (E = 210000 MPa,
// nu = 0.3 for S355), at that back-calculation's printed precision.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

// Model 10 "PROFIL-3", DBUSER H [0.9, 1.0, 0.03, 0.03, 1.0, 0.03] -- the second, symmetric
// geometry for the DBUSER H convention. MIDAS back-calc: A 0.0852, IH 0.01283976, IB 0.00500189,
// J 0.000025830, AsH 0.027000, AsB 0.050000.
TEST(MidasModel10Revision2, DbUserHSecondGeometry) {
  const auto p = resolveDbUserSection("H", {0.9, 1.0, 0.03, 0.03, 1.0, 0.03, 0.0, 0.0});
  rel(p.area, 0.08520000000000003, "A");
  rel(p.IH, 0.012839760000000037, "IH");
  rel(p.IB, 0.00500189, "IB");
  rel(p.J, 2.583e-05, "J");
  rel(p.AsH, 0.027, "AsH = tw*H");
  rel(p.AsB, 0.05, "AsB = 5/6*(B1*tf1+B2*tf2)");
  rel(p.IH, 0.01283976, "IH vs MIDAS", 1e-6);
  rel(p.IB, 0.00500189, "IB vs MIDAS", 1e-6);
  rel(p.J, 0.000025830, "J vs MIDAS", 5e-5);
}

// Model 10 "PROFIL-2": AISC10(US) W36X247. MIDAS back-calc: A 0.04677409..11, IH 0.006951065,
// IB 0.000420394, J 0.000014443, AsH 0.018942, AsB 0.023952.
TEST(MidasModel10Revision2, AiscW36x247) {
  const auto p = resolveCatalogSection("H", "AISC10(US)", "W36X247");
  rel(p.area, 0.0467741, "A = 72.5 in^2");
  rel(p.IH, 0.0069510648075199995, "IH = Ix 16700 in^4");
  rel(p.IB, 0.00042039373985599995, "IB = Iy 1010 in^4");
  rel(p.J, 1.444323046832e-05, "J = 34.7 in^4");
  rel(p.AsH, 0.0189418976, "AsH = d*tw");
  rel(p.AsB, 0.023951565, "AsB = 5/6*2*bf*tf");
  rel(p.area, 0.0467741, "A vs MIDAS", 1e-6);
  rel(p.IH, 0.006951065, "IH vs MIDAS", 1e-6);
  rel(p.IB, 0.000420394, "IB vs MIDAS", 2e-6);
  rel(p.J, 0.000014443, "J vs MIDAS", 5e-5);
  rel(p.AsH, 0.018942, "AsH vs MIDAS", 5e-5);
  rel(p.AsB, 0.023952, "AsB vs MIDAS", 5e-5);
}

// Model 10 "PROFIL-4": AISC10(US) HP14X89. MIDAS back-calc: A 0.01683868, IH 0.000376273,
// IB 0.000135691, J 0.000001494, AsH 0.005475, AsB 0.009721 (the latter two vary in the 4th digit
// between elements -- the J and shear checks use the back-calculation's printed precision).
TEST(MidasModel10Revision2, AiscHp14x89) {
  const auto p = resolveCatalogSection("H", "AISC10(US)", "HP14X89");
  rel(p.area, 0.016838676, "A = 26.1 in^2");
  rel(p.IH, 0.0003762732087424, "IH = Ix 904 in^4");
  rel(p.IB, 0.00013569144474559998, "IB = Iy 326 in^4");
  rel(p.J, 1.4942708179039999e-06, "J = 3.59 in^4");
  rel(p.AsH, 0.00547547292, "AsH = d*tw");
  rel(p.AsB, 0.009720948299999999, "AsB = 5/6*2*bf*tf");
  rel(p.area, 0.01683868, "A vs MIDAS", 1e-6);
  rel(p.IH, 0.000376273, "IH vs MIDAS", 2e-6);
  rel(p.IB, 0.000135691, "IB vs MIDAS", 5e-6);
  rel(p.J, 0.000001494, "J vs MIDAS", 5e-4);
  rel(p.AsH, 0.005475, "AsH vs MIDAS", 2e-4);
  rel(p.AsB, 0.009721, "AsB vs MIDAS", 1e-4);
}

TEST(MidasModel10Revision2, UnknownAiscShapeIsRejectedNotGuessed) {
  EXPECT_THROW(resolveCatalogSection("H", "AISC10(US)", "W14X90"), std::invalid_argument);
  EXPECT_THROW(resolveCatalogSection("H", "AISC05(US)", "W36X247"), std::invalid_argument);
}
