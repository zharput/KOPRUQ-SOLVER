// 2026-11-20 -- full AISC rolled I-shape catalog (W, HP, M, S) for MIDAS DB_NAME "AISC10(US)".
// Dedicated new file. Expected literals: independent Python reading of the same AISC Shapes
// Database v16.0 rows (steelpy 1.1.1 CSV files) with 1 in = 0.0254 m exactly, plus MIDAS's I-shape
// shear-area convention (AsH = d*tw, AsB = 5/6*2*bf*tf; confirmed on model 10).
#include <gtest/gtest.h>
#include <cmath>
#include <set>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-12) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
struct Expect { const char* name; double A, IH, IB, J, AsH, AsB, H, B; };
const Expect kRows[] = {
    {"W14X90", 0.01709674, 0.00041581519417439996, 0.0001506757760672, 1.6898995879359998e-06, 0.0039741856, 0.011069870333333334, 0.3556, 0.3683},
    {"HP12X53", 0.00999998, 0.00016357895026079998, 5.2861391051199995e-05, 4.66179196672e-07, 0.00331160628, 0.0056128919999999995, 0.29972, 0.3048},
    {"M12.5X12.4", 0.0023419307999999997, 3.7169466306079995e-05, 8.366251654559998e-07, 2.0520209282079996e-08, 0.0012499975, 0.000919353, 0.3175, 0.09525},
    {"S24X121", 0.02290318, 0.0013152913048959998, 3.45472083248e-05, 5.32776224768e-06, 0.012645136000000001, 0.009434927366666668, 0.6223, 0.20447},
};
}

TEST(MidasAiscCatalog, OneShapeOfEachSeriesMatchesTheTabulatedRow) {
  for (const auto& e : kRows) {
    const auto p = resolveCatalogSection("H", "AISC10(US)", e.name);
    rel(p.area, e.A, e.name, 1e-12);
    rel(p.IH, e.IH, e.name, 1e-12);
    rel(p.IB, e.IB, e.name, 1e-12);
    rel(p.J, e.J, e.name, 1e-12);
    rel(p.AsH, e.AsH, e.name, 1e-12);
    rel(p.AsB, e.AsB, e.name, 1e-12);
    rel(p.extentH, e.H, e.name, 1e-12);
    rel(p.extentB, e.B, e.name, 1e-12);
  }
}

TEST(MidasAiscCatalog, TableIsSortedUniqueAndComplete) {
  const auto& t = aisc::kIShapes;
  EXPECT_EQ(t.size(), 355u); // W 289 + HP 22 + M 16 + S 28
  std::set<std::string_view> seen;
  for (std::size_t i = 0; i < t.size(); ++i) {
    EXPECT_TRUE(seen.insert(t[i].name).second) << "duplicate " << t[i].name;
    if (i) EXPECT_LT(t[i - 1].name, t[i].name) << "binary search needs a sorted table";
    EXPECT_GT(t[i].A, 0); EXPECT_GT(t[i].Ix, 0); EXPECT_GT(t[i].Iy, 0); EXPECT_GT(t[i].J, 0);
    EXPECT_GT(t[i].d, 0); EXPECT_GT(t[i].bf, 0); EXPECT_GT(t[i].tw, 0); EXPECT_GT(t[i].tf, 0);
  }
}

TEST(MidasAiscCatalog, Model10ShapesUnchangedByTheTable) {
  // The two shapes verified against MIDAS on model 10 must give exactly what they gave before.
  rel(resolveCatalogSection("H", "AISC10(US)", "W36X247").IH, 0.0069510648075199995, "W36X247 IH");
  rel(resolveCatalogSection("H", "AISC10(US)", "HP14X89").J, 1.4942708179039999e-06, "HP14X89 J");
}

TEST(MidasAiscCatalog, NonIShapesOfTheDatabaseAreStillRejected) {
  EXPECT_THROW(resolveCatalogSection("C", "AISC10(US)", "C10X30"), std::invalid_argument);
  EXPECT_THROW(resolveCatalogSection("L", "AISC10(US)", "L4X4X1/2"), std::invalid_argument);
  EXPECT_THROW(resolveCatalogSection("T", "AISC10(US)", "WT8X50"), std::invalid_argument);
}
