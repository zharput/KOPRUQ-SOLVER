// 2026-11-20 -- MIDAS import: (a) a planar model with STYP = 0 is analysed as full 3D (the former
// zero-extent auto-restraint heuristic is removed); (b) "Center-Top" section offsets (OFFSET_PT
// "CT") become rigid member-end offsets to the centroid. Dedicated new file.
//
// Expectations are derived independently from beam theory (Euler-Bernoulli: USE_SHEAR_DEFORM
// false), for a 4 m cantilever along +X fixed at node 1, SB section H = 0.3 (local z), B = 1.0
// (local y): Iy = B H^3/12 = 0.00225, Iz = H B^3/12 = 0.025, J (Roark rectangle) = 0.007300148175,
// E = 22000 ((35+8)/10)^0.3 MPa = 34077146.19918933 kPa, G = E / 2.4. For "CT" the member axis lies
// e = H/2 = 0.15 below the nodes:
//  * tip load Fz = -P: the axis bends (theta_y = +P L^2 / (2 E Iy) at the tip, hogging), the top
//    fiber -- where the node is -- elongates: tip DX = theta_y e = +P L^2 e / (2 E Iy) (cf. MIDAS
//    model 03: +DX under a downward tip load);
//  * tip load Fy = +P applied e above the axis: torque about +x = -P e, tip RX = -P e L / (G J)
//    (cf. MIDAS model 02: RX < 0 under +FY).
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using namespace kopruq::io::midas;

namespace {
constexpr double kE = 34077146.19918933, kG = kE / 2.4, kL = 4.0, kP = 100.0;
constexpr double kIy = 1.0 * 0.3 * 0.3 * 0.3 / 12, kIz = 0.3 * 1.0 / 12, kJ = 0.007300148174999999, kE0 = 0.15;

std::string sect(const std::string& offsetFields) {
  return R"({"SECTTYPE":"DBUSER","SECT_NAME":"S","SECT_BEFORE":{)" + offsetFields +
         R"("SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[0.3,1.0]}}})";
}
// Planar model (all nodes on the X axis), no STYP record unless given in `extra`.
nlohmann::json model(const std::string& section, const std::string& load, const std::string& extra = "") {
  const std::string text = R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0}},
    "MATL":{"1":{"TYPE":"CONC","NAME":"C35/45"}},
    "MATD":{"1":{"TYPE":"CONC","NAME":"C35/45","DATA1":{"CODENAME":"EN04(RC)","DESIGN":{"C_FC":35000.0}}}},
    "SECT":{"1":)json" + section + R"json(},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "STLD":{"1":{"NO":1,"NAME":"F","TYPE":"D"}},
    "CNLD":{"2":{"ITEMS":[{"ID":1,"LCNAME":"F",)json" + load + R"json(}]}})json" + extra + "}}";
  return nlohmann::json::parse(text);
}
kopruq::NodeResults tip(const MidasImportResult& r) {
  auto sol = kopruq::LinearStaticSolver::solveCases(r.input.model, r.input.restraints, r.input.load_cases);
  EXPECT_TRUE(sol.status.success) << sol.status.message;
  for (const auto& n : sol.cases.at(0).results.nodes) if (n.node_id == 2) return n;
  ADD_FAILURE() << "node 2 missing";
  return {};
}
bool rejected(const MidasImportResult& r, const std::string& prefix) {
  for (const auto& m : r.report.rejected) if (m.rfind(prefix, 0) == 0) return true;
  return false;
}
const std::string kFy = R"("FX":0.0,"FY":100.0,"FZ":0.0,"MX":0.0,"MY":0.0,"MZ":0.0)";
const std::string kFz = R"("FX":0.0,"FY":0.0,"FZ":-100.0,"MX":0.0,"MY":0.0,"MZ":0.0)";
}

TEST(MidasPlanarModel, Styp0PlanarModelIsFull3D) {
  const auto r = importMidasJson(model(sect(R"("OFFSET_PT":"CC",)"), kFy));
  EXPECT_EQ(r.input.restraints.count(2), 0u) << "no out-of-plane auto-restraint for a STYP=0 model";
  const auto t = tip(r);
  EXPECT_NEAR(t.displacement[1], kP * kL * kL * kL / (3 * kE * kIz), 1e-12) << "DY = P L^3 / (3 E Iz)";
  EXPECT_NEAR(t.displacement[3], 0.0, 1e-15) << "no torsion without an offset";
}

TEST(MidasSectionOffset, CenterTopDownwardLoadMovesTopFiberNodeAxially) {
  const auto r = importMidasJson(model(sect(R"("OFFSET_PT":"CT","OFFSET_CENTER":0,"USER_OFFSET_REF":0,"HORZ_OFFSET_OPT":0,"VERT_OFFSET_OPT":0,"USERDEF_OFFSET_YI":0.0,"USERDEF_OFFSET_ZI":0.0,)"), kFz));
  ASSERT_EQ(r.input.model.beams.size(), 1u);
  EXPECT_NEAR(r.input.model.beams[0].offset1.z(), -kE0, 1e-15);
  EXPECT_NEAR(r.input.model.beams[0].offset2.z(), -kE0, 1e-15);
  const auto t = tip(r);
  EXPECT_NEAR(t.displacement[0], kP * kL * kL * kE0 / (2 * kE * kIy), 1e-12) << "DX = +P L^2 e / (2 E Iy)";
  EXPECT_GT(t.displacement[0], 0.0);
  EXPECT_NEAR(t.displacement[2], -kP * kL * kL * kL / (3 * kE * kIy), 1e-12) << "DZ unchanged by the offset";
}

TEST(MidasSectionOffset, CenterTopHorizontalLoadTwistsTheMember) {
  const auto r = importMidasJson(model(sect(R"("OFFSET_PT":"CT",)"), kFy));
  const auto t = tip(r);
  EXPECT_NEAR(t.displacement[3], -kP * kE0 * kL / (kG * kJ), 1e-12) << "RX = -P e L / (G J)";
  EXPECT_LT(t.displacement[3], 0.0);
}

TEST(MidasSectionOffset, UnverifiedOffsetsAreRejectedNotGuessed) {
  const auto cb = importMidasJson(model(sect(R"("OFFSET_PT":"CB",)"), kFz));
  EXPECT_TRUE(cb.input.model.sections.empty());
  EXPECT_TRUE(rejected(cb, "section 1: section offset point 'CB'"));
  const auto user = importMidasJson(model(sect(R"("OFFSET_PT":"CC","USERDEF_OFFSET_ZI":0.1,)"), kFz));
  EXPECT_TRUE(user.input.model.sections.empty());
  EXPECT_TRUE(rejected(user, "section 1: user-defined section offset"));
}
