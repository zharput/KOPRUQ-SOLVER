// 2026-11-21 -- MIDAS BMLD element beam loads beyond uniform GZ forces (verification model 23):
// multi-point trapezoidal loads (D/P with up to 4 points, piecewise linear), uniform/trapezoidal
// moments (UNIMOMENT), concentrated moments and multiple concentrated loads, local directions, and
// explicit rejection of projection / eccentricity / pressure. Dedicated new file.
// Expected values: hand statics (resultants of the piecewise-linear intensity) and the closed-form
// cantilever under a uniform distributed moment m about z: base MZ = -m L, tip rotation
// m L^2 / (2 E Iz) (Euler-Bernoulli; nodal values are exact with consistent loads).
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using namespace kopruq::io::midas;

namespace {
constexpr double L = 4.0;
nlohmann::json model(const std::string& items) {
  return nlohmann::json::parse(std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "BMLD":{"1":{"ITEMS":[)json") + items + R"json(]}}}})json");
}
std::string item(const char* lc, const char* type, const char* dir, const char* d, const char* p, const char* extra = "") {
  return std::string(R"({"ID":1,"LCNAME":")") + lc + R"(","GROUP_NAME":"","CMD":"BEAM","TYPE":")" + type + R"(","DIRECTION":")" + dir +
         R"(","USE_PROJECTION":false,"USE_ECCEN":false,"D":)" + d + R"(,"P":)" + p + R"(,"USE_ADDITIONAL":false)" + extra + "}";
}
kopruq::StaticSolution solveFirst(const MidasImportResult& r) {
  auto s = kopruq::LinearStaticSolver::solveCases(r.input.model, r.input.restraints, r.input.load_cases);
  EXPECT_TRUE(s.status.success) << s.status.message;
  return s.cases.front();
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, kopruq::Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
bool anyContains(const std::vector<std::string>& v, const char* s) {
  for (const auto& x : v) if (x.find(s) != std::string::npos) return true;
  return false;
}
}

TEST(MidasBmldExtended, FourPointTrapezoidIsPiecewiseLinear) {
  // Model 23, elements 16/31: D [0, .3, .7, 1], P [-200, -500, -1000, -100].
  const auto r = importMidasJson(model(item("T", "UNILOAD", "GZ", "[0.0,0.3,0.7,1.0]", "[-200.0,-500.0,-1000.0,-100.0]")));
  ASSERT_EQ(r.input.load_cases.size(), 1u);
  const auto& ml = r.input.load_cases[0].member;
  ASSERT_EQ(ml.size(), 3u);
  EXPECT_DOUBLE_EQ(ml[1].start, 0.3); EXPECT_DOUBLE_EQ(ml[1].end, 0.7);
  EXPECT_DOUBLE_EQ(ml[1].w1, -500.0); EXPECT_DOUBLE_EQ(ml[1].w2, -1000.0);
  // Resultant: L [0.3 (-350) + 0.4 (-750) + 0.3 (-550)] = -570 L -> base FZ = +570 L.
  const auto s = solveFirst(r);
  EXPECT_NEAR(nodeOf(s, 1).reaction[2], 570.0 * L, 1e-6);
}

TEST(MidasBmldExtended, ThreePointTrapezoidStopsWhereDStopsIncreasing) {
  // Model 23, element 88: D [0, .8, 1, 0], P [-750, -1000, -850, 0] -> two segments.
  const auto r = importMidasJson(model(item("T", "UNILOAD", "GZ", "[0.0,0.8,1.0,0.0]", "[-750.0,-1000.0,-850.0,0.0]")));
  ASSERT_EQ(r.input.load_cases[0].member.size(), 2u);
  // Resultant: L [0.8 (-875) + 0.2 (-925)] = -885 L.
  const auto s = solveFirst(r);
  EXPECT_NEAR(nodeOf(s, 1).reaction[2], 885.0 * L, 1e-6);
}

TEST(MidasBmldExtended, UniformMomentAboutGlobalZ) {
  // Model 23, elements 16/31 case POINT: UNIMOMENT GZ, P [100, 100], D [0, 1].
  const auto r = importMidasJson(model(item("M", "UNIMOMENT", "GZ", "[0.0,1.0,0.0,0.0]", "[100.0,100.0,0.0,0.0]")));
  const auto& ml = r.input.load_cases.at(0).member;
  ASSERT_EQ(ml.size(), 1u);
  EXPECT_EQ(ml[0].kind, kopruq::MemberLoadKind::Moment);
  EXPECT_EQ(ml[0].axes, kopruq::LoadAxes::Global);
  EXPECT_EQ(ml[0].direction, 2);
  const auto s = solveFirst(r);
  const double m = 100.0, EIz = r.input.model.materials[0].elastic_modulus * r.input.model.sections[0].Iz;
  EXPECT_NEAR(nodeOf(s, 1).reaction[5], -m * L, 1e-6) << "base MZ = -m L";
  for (int i = 0; i < 3; ++i) EXPECT_NEAR(nodeOf(s, 1).reaction[i], 0.0, 1e-6);
  EXPECT_NEAR(nodeOf(s, 2).displacement[5], m * L * L / (2 * EIz), 1e-9 * m * L * L / EIz) << "tip RZ";
}

TEST(MidasBmldExtended, ConcentratedLoadsAllPointsAndLocalDirection) {
  const auto r = importMidasJson(model(item("C", "CONLOAD", "LY", "[0.25,0.75,0.0,0.0]", "[10.0,20.0,0.0,0.0]") + "," +
                                       item("C", "CONMOMENT", "GX", "[0.5,0.0,0.0,0.0]", "[7.0,0.0,0.0,0.0]")));
  const auto& pl = r.input.load_cases.at(0).point;
  ASSERT_EQ(pl.size(), 3u);
  EXPECT_EQ(pl[0].axes, kopruq::LoadAxes::Local); EXPECT_EQ(pl[0].direction, 1);
  EXPECT_DOUBLE_EQ(pl[1].position, 0.75); EXPECT_DOUBLE_EQ(pl[1].value, 20.0);
  EXPECT_EQ(pl[2].kind, kopruq::MemberLoadKind::Moment); EXPECT_EQ(pl[2].direction, 0);
  const auto s = solveFirst(r);
  // Local y = global Y for this element: base FY = -30, MZ = -(1 * 10 + 3 * 20) = -70, MX = -7.
  EXPECT_NEAR(nodeOf(s, 1).reaction[1], -30.0, 1e-8);
  EXPECT_NEAR(nodeOf(s, 1).reaction[5], -70.0, 1e-8);
  EXPECT_NEAR(nodeOf(s, 1).reaction[3], -7.0, 1e-8);
}

TEST(MidasBmldExtended, UnverifiedOptionsAreRejected) {
  auto proj = importMidasJson(model(std::string(R"({"ID":1,"LCNAME":"X","CMD":"BEAM","TYPE":"UNILOAD","DIRECTION":"GZ","USE_PROJECTION":true,"USE_ECCEN":false,"D":[0,1,0,0],"P":[-1,-1,0,0]})")));
  EXPECT_TRUE(anyContains(proj.report.rejected, "USE_PROJECTION"));
  EXPECT_EQ(proj.report.member_loads, 0u);
  auto ecc = importMidasJson(model(std::string(R"({"ID":1,"LCNAME":"X","CMD":"BEAM","TYPE":"UNILOAD","DIRECTION":"GZ","USE_PROJECTION":false,"USE_ECCEN":true,"D":[0,1,0,0],"P":[-1,-1,0,0]})")));
  EXPECT_TRUE(anyContains(ecc.report.rejected, "USE_ECCEN"));
  auto pres = importMidasJson(model(item("X", "PRESSURE", "GZ", "[0.0,1.0,0.0,0.0]", "[-1.0,-1.0,0.0,0.0]")));
  EXPECT_TRUE(anyContains(pres.report.rejected, "unsupported CMD/TYPE/DIRECTION"));
}
