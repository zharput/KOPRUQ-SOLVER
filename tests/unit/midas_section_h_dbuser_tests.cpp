// 2026-11-20 -- MIDAS NX DBUSER "H" section (I/H with possibly unequal flanges),
// vSIZE [H, B1, tw, tf1, B2, tf2, r1, r2]. Dedicated new file (CLAUDE.md: never overwrite or
// truncate an existing test file to add tests).
//
// Expected values are computed independently (Python, outside this code base; IH via first/second
// moments of the three strips about the top fiber, not the production parallel-axis sum) and
// written as literals. For verification model 10's section "PROFIL" they are ALSO what MIDAS itself
// uses: each property back-calculated per element from MIDAS's own displacement and beam-force
// exports (nodal-load cases FORCE-2/FORCE-3, four elements, 2-node Timoshenko fit residual ~1e-7):
//   A 0.06340000, IH 0.010876745, IB 0.001213113, J 0.000032060, AsH 0.020000, AsB 0.037500.
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
constexpr double kMidasBackCalc = 5e-6; // MIDAS back-calculated values are given to 6+ digits
}

TEST(MidasDbUserHSection, Model10ProfilMatchesIndependentValues) {
  const auto p = unequalFlangeISection(1.0, 0.5, 0.02, 0.03, 0.6, 0.05);
  rel(p.area, 0.06340000000000003, "A");
  rel(p.IH, 0.010876745115667736, "IH (strong axis, about centroid)");
  rel(p.IB, 0.0012131133333333332, "IB (weak axis)");
  rel(p.J, 3.206000000000001e-05, "J (thin strips, web flange-midline to flange-midline)");
  rel(p.AsH, 0.02, "AsH = tw*H");
  rel(p.AsB, 0.0375, "AsB = 5/6*(B1*tf1 + B2*tf2)");
}

TEST(MidasDbUserHSection, Model10ProfilMatchesMidasBackCalculation) {
  const auto p = unequalFlangeISection(1.0, 0.5, 0.02, 0.03, 0.6, 0.05);
  rel(p.area, 0.06340000, "A", kMidasBackCalc);
  rel(p.IH, 0.010876745, "IH", kMidasBackCalc);
  rel(p.IB, 0.001213113, "IB", kMidasBackCalc);
  rel(p.J, 0.000032060, "J", kMidasBackCalc);
  rel(p.AsH, 0.020000, "AsH", kMidasBackCalc);
  rel(p.AsB, 0.037500, "AsB", kMidasBackCalc);
}

// Equal flanges: IH must equal the textbook symmetric-I closed form (B H^3 - (B-tw) Hi^3)/12.
TEST(MidasDbUserHSection, EqualFlangesReduceToSymmetricIClosedForm) {
  const auto p = unequalFlangeISection(0.8, 0.4, 0.015, 0.025, 0.4, 0.025);
  rel(p.IH, 0.003531510416666673, "IH symmetric closed form", 1e-12);
  rel(p.area, 0.031250000000000014, "A");
  rel(p.IB, 0.00026687760416666673, "IB");
  rel(p.J, 5.038541666666668e-06, "J");
  rel(p.AsH, 0.012, "AsH");
  rel(p.AsB, 0.01666666666666667, "AsB");
}

// Flipping the section upside down (swap the two flanges) changes no section property.
TEST(MidasDbUserHSection, FlangeSwapInvariance) {
  const auto a = unequalFlangeISection(1.0, 0.5, 0.02, 0.03, 0.6, 0.05);
  const auto b = unequalFlangeISection(1.0, 0.6, 0.02, 0.05, 0.5, 0.03);
  rel(b.area, a.area, "A"); rel(b.IH, a.IH, "IH"); rel(b.IB, a.IB, "IB");
  rel(b.J, a.J, "J"); rel(b.AsH, a.AsH, "AsH"); rel(b.AsB, a.AsB, "AsB");
}

TEST(MidasDbUserHSection, DispatchAndZeroFilletsAccepted) {
  const auto p = resolveDbUserSection("H", {1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0.0, 0.0, 0.0, 0.0});
  rel(p.IH, 0.010876745115667736, "IH via dispatch");
  rel(p.J, 3.206000000000001e-05, "J via dispatch");
}

TEST(MidasDbUserHSection, RejectsFilletsAndInvalidDimensions) {
  EXPECT_THROW(resolveDbUserSection("H", {1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0.01, 0.0}), std::invalid_argument);
  EXPECT_THROW(resolveDbUserSection("H", {1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0.0, 0.01}), std::invalid_argument);
  EXPECT_THROW(resolveDbUserSection("H", {1.0, 0.5, 0.02, 0.03, 0.6}), std::invalid_argument); // too few values
  EXPECT_THROW(unequalFlangeISection(1.0, 0.5, 0.02, 0.5, 0.6, 0.5), std::invalid_argument);   // tf1+tf2 >= H
  EXPECT_THROW(unequalFlangeISection(1.0, 0.5, 0.7, 0.03, 0.6, 0.05), std::invalid_argument);  // tw > B1
  EXPECT_THROW(unequalFlangeISection(1.0, 0.0, 0.02, 0.03, 0.6, 0.05), std::invalid_argument); // B1 = 0
}
