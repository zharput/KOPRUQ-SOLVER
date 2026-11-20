// 2026-11-20 -- verification model 10, second revision (user added H sections and an S355 steel
// material): a second DBUSER "H" geometry (the AISC10(US) catalog part was removed 2026-11-20).
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

// The two AISC10(US) catalog tests (AiscW36x247, AiscHp14x89) and UnknownAiscShapeIsRejectedNotGuessed
// that followed here were removed on 2026-11-20 together with the catalog itself (user decision:
// precast concrete bridges first; steel profiles only as user-defined I/H or T sections). Catalog
// sections are now rejected by the importer -- see midas_catalog_rejected_tests.cpp.
