// 2026-11-20 -- user-defined steel profiles (catalogs removed): H/I with web-to-flange fillets r1,
// B2 = tf2 = 0 meaning a symmetric section, user channel "C", user angle "L" (truss members only).
// Dedicated new file. Expected literals: independent Python strip integration (200000 strips) of the
// exact geometry, and the values MIDAS itself used in verification model 20 (back-calculated per
// element from MIDAS's own results: H A 0.00542514, IH 2.4920e-5, AsH 0.001280; C A 0.001348;
// L A 0.000475).
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
}

TEST(MidasUserProfiles, HWithFilletsMatchesStripIntegrationAndMidas) {
  // Model 20 "PROF-I": [0.16, 0.16, 0.008, 0.013, 0, 0, r1 = 0.015] (B2 = tf2 = 0 -> symmetric).
  const auto p = resolveDbUserSection("H", {0.16, 0.16, 0.008, 0.013, 0.0, 0.0, 0.015, 0.0});
  rel(p.area, 0.005425141652942296, "A = 2 b tf + (h-2tf) tw + (4-pi) r^2", 1e-12);
  rel(p.IH, 2.49200101115416e-05, "IH (independent strips)", 1e-7);
  rel(p.IB, 8.892347353940925e-06, "IB (independent strips)", 1e-7);
  rel(p.J, 2.594346666666667e-07, "J without fillets (MIDAS manual)", 1e-12);
  rel(p.AsH, 0.00128, "AsH = H tw (fillets excluded)", 1e-12);
  rel(p.area, 0.00542514, "A vs MIDAS", 2e-6);
  rel(p.IH, 2.4920e-05, "IH vs MIDAS", 5e-6);
  rel(p.topFiber, 0.08, "symmetric: centroid at mid-depth", 1e-12);
}

TEST(MidasUserProfiles, EmptyBottomFlangeMeansSymmetric) {
  const auto a = resolveDbUserSection("H", {0.16, 0.16, 0.008, 0.013, 0.0, 0.0, 0.0, 0.0});
  const auto b = resolveDbUserSection("H", {0.16, 0.16, 0.008, 0.013, 0.16, 0.013, 0.0, 0.0});
  rel(a.area, b.area, "A", 1e-15); rel(a.IH, b.IH, "IH", 1e-15); rel(a.IB, b.IB, "IB", 1e-15); rel(a.J, b.J, "J", 1e-15);
  EXPECT_THROW(resolveDbUserSection("H", {0.16, 0.16, 0.008, 0.013, 0.16, 0.0}), std::invalid_argument); // only one of B2/tf2 empty
  EXPECT_THROW(resolveDbUserSection("H", {0.16, 0.16, 0.008, 0.013, 0.0, 0.0, 0.0, 0.01}), std::invalid_argument); // r2 unsupported
}

TEST(MidasUserProfiles, ChannelInPlaneLikeIAndOwnWeakAxis) {
  // Model 20 "PROF-U": [0.1, 0.05, 0.006, 0.0085, 0.05, 0.0085].
  const auto c = resolveDbUserSection("C", {0.1, 0.05, 0.006, 0.0085, 0.05, 0.0085, 0.0, 0.0});
  const auto i = resolveDbUserSection("H", {0.1, 0.05, 0.006, 0.0085, 0.05, 0.0085, 0.0, 0.0});
  rel(c.area, 0.001348, "A (MIDAS 0.001348)", 1e-12);
  rel(c.IH, i.IH, "IH as the I section", 1e-15);
  rel(c.AsH, 0.1 * 0.006, "AsH = H tw", 1e-12);
  rel(c.IB, 3.3056338673877543e-07, "IB about the channel's own centroid (independent strips)", 1e-7);
}

TEST(MidasUserProfiles, AngleIsTrussOnly) {
  const auto l = resolveDbUserSection("L", {0.05, 0.05, 0.005, 0.005});
  rel(l.area, 0.000475, "A = H tw + (B - tw) tf (MIDAS 0.000475)", 1e-12);
  EXPECT_TRUE(l.axialOnly);
  auto model = [](const char* type) {
    return nlohmann::json::parse(std::string(R"json({"DBG":{
      "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
      "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":2.0,"Y":0.0,"Z":0.0},"3":{"X":0.0,"Y":1.0,"Z":1.0}},
      "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
      "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"L","SECT_BEFORE":{"SHAPE":"L","USE_SHEAR_DEFORM":true,"SECT_I":{"vSIZE":[0.05,0.05,0.005,0.005,0,0]}}}},
      "ELEM":{"1":{"TYPE":")json") + type + R"json(","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
      "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]},"3":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}}}})json");
  };
  const auto truss = importMidasJson(model("TRUSS"));
  EXPECT_EQ(truss.input.model.beams.size(), 1u);
  const auto beam = importMidasJson(model("BEAM"));
  EXPECT_TRUE(beam.input.model.beams.empty());
  bool found = false;
  for (const auto& m : beam.report.rejected) if (m.find("BEAM with an angle (L) section") != std::string::npos) found = true;
  EXPECT_TRUE(found);
}
