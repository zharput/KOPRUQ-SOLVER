// 2026-11-20 -- verification model 22: MIDAS PSC 1CEL box section, beam temperature gradient
// (GTMP) and structure type (STYP). Dedicated new file (CLAUDE.md: never overwrite or truncate an
// existing test file to add tests).
//
// Independent expectations:
//  * PSC geometry: A, Iy, Iz from a separate Python polygon integration of the same manual-diagram
//    geometry (A 10.58, Iy 13.67209341262339, Iz 68.70453333333333) -- and MIDAS's own Section
//    Properties table for model 22's box (A 10.58, Iyy 13.67209, Izz 68.70453).
//  * PSC J / shear areas: an independent Python FE implementation (scipy Delaunay mesh, same
//    equations) at h = 0.0125, 71308 nodes: J 27.850230, Asy 5.421686, Asz 3.653751; MIDAS's own (coarser-mesh) values J 27.92024, Asy 5.434952,
//    Asz 3.681402 are checked within 1%.
//  * Gradient sign, from physics alone (not from either program's convention): a cantilever whose
//    +z face is hotter (MIDAS TZ = T2z - T1z > 0) elongates on top and curls DOWN -- tip
//    w = -kappa L^2 / 2, kappa = alpha TZ / h; likewise TY > 0 (+y face hotter) moves the tip to -y.
//  * STYP = 1 (X-Z plane): UY, RX, RZ restrained at every node.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
const Psc1CellDims kModel22Box{0.25, 0.4, 2.5, 2.0, 0.0, 3.0, 0.25, 0.4, 2.0, 0.2, 0.3, 2.2, 2.2};

// Minimal MIDAS-shaped model: node 1 fixed, node 2 free, one 4 m beam along +X (local z = +Z,
// local y = +Y at ANGLE 0); `sect` is the SECT record, `extra` appended records. Node 3 is a
// fully restrained, unconnected node off the X axis: it only makes the model non-planar, so the
// importer's planar-model auto-restraint (zero bounding-box extent) does not apply.
nlohmann::json model(const std::string& sect, const std::string& extra) {
  const std::string text = R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0},"3":{"X":0.0,"Y":1.0,"Z":1.0}},
    "MATL":{"1":{"TYPE":"CONC","NAME":"C35/45"}},
    "MATD":{"1":{"TYPE":"CONC","NAME":"C35/45","DATA1":{"CODENAME":"EN04(RC)","DESIGN":{"C_FC":35000.0}}}},
    "SECT":{"1":)json" + sect + R"json(},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]},"3":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "STLD":{"1":{"NO":1,"NAME":"TG","TYPE":"T"}})json" + extra + "}}";
  return nlohmann::json::parse(text);
}
const std::string kSB = R"({"SECTTYPE":"DBUSER","SECT_NAME":"S","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}})"; // H = 1.0, B = 0.5
std::string psc(const std::string& shape, const std::string& joint) {
  return R"({"SECTTYPE":"PSC","SECT_NAME":"BOX","SECT_BEFORE":{"OFFSET_PT":"CC","USE_SHEAR_DEFORM":true,"SHAPE":")" + shape +
         R"(","SECT_I":{"vSIZE_PSC_A":[0.25,0.4,0.0,0.0,2.5,0.0],"vSIZE_PSC_B":[2.0,0.0,0.0,0.0,0.0,3.0],)"
         R"("vSIZE_PSC_C":[0.25,0.4,0.0,0.0,2.0,0.0,0.2,0.0,0.0,0.3],"vSIZE_PSC_D":[2.2,0.0,0.0,0.0,2.2,0.0,0.0,0.0],"SWIDTH":0.25},)"
         R"("USE_SYMMETRIC":true,"USE_SMALL_HOLE":false,"USE_USER_INTPUT_STIFF":false,"JOINT":[)" + joint + "]}}";
}
bool hasMessage(const std::vector<std::string>& v, const std::string& prefix) {
  for (const auto& m : v) if (m.rfind(prefix, 0) == 0) return true;
  return false;
}
kopruq::NodeResults tipOf(const MidasImportResult& r) {
  auto sol = kopruq::LinearStaticSolver::solveCases(r.input.model, r.input.restraints, r.input.load_cases);
  EXPECT_TRUE(sol.status.success) << sol.status.message;
  for (const auto& n : sol.cases.at(0).results.nodes) if (n.node_id == 2) return n;
  ADD_FAILURE() << "node 2 missing";
  return {};
}
}

TEST(MidasPsc1Cell, Model22BoxGeometryMatchesIndependentAndMidasValues) {
  const auto p = psc1CellSection(kModel22Box);
  rel(p.area, 10.579999999999998, "A (independent polygon)", 1e-12);
  rel(p.IH, 13.67209341262339, "Iy (independent polygon)", 1e-12);
  rel(p.IB, 68.70453333333333, "Iz (independent polygon)", 1e-12);
  rel(p.IH, 13.67209, "Iyy vs MIDAS table", 1e-6);
  rel(p.IB, 68.70453, "Izz vs MIDAS table", 1e-6);
  rel(p.extentH, 3.15, "overall depth", 1e-12);
  rel(p.extentB, 10.0, "overall width", 1e-12);
}

TEST(MidasPsc1Cell, Model22BoxTorsionAndShearFromConvergedFE) {
  const auto p = psc1CellSection(kModel22Box);
  // Independent Python FE (h = 0.0125): see the file header.
  rel(p.J, 27.850230, "J vs independent FE", 5e-4);
  rel(p.AsB, 5.421686, "Asy (horizontal) vs independent FE", 5e-4);
  rel(p.AsH, 3.653751, "Asz (vertical) vs independent FE", 5e-4);
  // MIDAS's own values come from its coarser mesh and sit slightly above the converged ones.
  rel(p.J, 27.92024, "J vs MIDAS", 1e-2);
  rel(p.AsB, 5.434952, "Asy vs MIDAS", 1e-2);
  rel(p.AsH, 3.681402, "Asz vs MIDAS", 1e-2);
  EXPECT_LT(p.J, 27.92024);
  EXPECT_LT(p.AsH, 3.681402);
}

TEST(MidasPsc1Cell, RejectsInconsistentDimensions) {
  Psc1CellDims d = kModel22Box;
  d.HI3 = 2.1; // inner heights no longer add up to the outer height
  EXPECT_THROW(psc1CellSection(d), std::invalid_argument);
  d = kModel22Box;
  d.BI1 = d.BI3 = 3.2; // cell wider than the box
  EXPECT_THROW(psc1CellSection(d), std::invalid_argument);
}

TEST(MidasPsc1Cell, ImporterMapsPscArraysAndRejectsUnsupportedVariants) {
  const auto ok = importMidasJson(model(psc("1CEL", "false,false,false,false,false,false,false,false"), ""));
  ASSERT_EQ(ok.input.model.sections.size(), 1u);
  rel(ok.input.model.sections[0].area, 10.58, "A", 1e-12);
  rel(ok.input.model.sections[0].Iy, 13.67209341262339, "Iy", 1e-12);
  const auto joint = importMidasJson(model(psc("1CEL", "true,false,false,false,false,false,false,false"), ""));
  EXPECT_TRUE(joint.input.model.sections.empty());
  EXPECT_TRUE(hasMessage(joint.report.rejected, "section 1: PSC 1CEL with a joint"));
  const auto twoCell = importMidasJson(model(psc("2CEL", "false,false,false,false,false,false,false,false"), ""));
  EXPECT_TRUE(twoCell.input.model.sections.empty());
  EXPECT_TRUE(hasMessage(twoCell.report.rejected, "section 1: unsupported PSC shape '2CEL'"));
}

TEST(MidasGradientTemperature, UseSectionDimensionsAndSignMapping) {
  const auto r = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":1,"TZ":20.0,"USE_HZ":true,"HZ":0.0,"TY":8.0,"USE_HY":true,"HY":0.0}]}})"));
  ASSERT_EQ(r.input.load_cases.size(), 1u);
  ASSERT_EQ(r.input.load_cases[0].thermal.size(), 1u);
  const auto& t = r.input.load_cases[0].thermal[0];
  EXPECT_DOUBLE_EQ(t.gradient_z_delta_t, -20.0); // KOPRUQ convention is the opposite difference
  EXPECT_DOUBLE_EQ(t.gradient_z_depth, 1.0);     // SB H
  EXPECT_DOUBLE_EQ(t.gradient_y_delta_t, -8.0);
  EXPECT_DOUBLE_EQ(t.gradient_y_depth, 0.5);     // SB B
  EXPECT_DOUBLE_EQ(t.uniform_delta_t, 0.0);
}

TEST(MidasGradientTemperature, ExplicitDepthsWhenNotUsingSectionDimensions) {
  const auto r = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":1,"TZ":20.0,"USE_HZ":false,"HZ":2.0,"TY":0.0,"USE_HY":true,"HY":0.0}]}})"));
  const auto& t = r.input.load_cases.at(0).thermal.at(0);
  EXPECT_DOUBLE_EQ(t.gradient_z_depth, 2.0);
  EXPECT_DOUBLE_EQ(t.gradient_y_delta_t, 0.0);
  EXPECT_DOUBLE_EQ(t.gradient_y_depth, 0.0);
}

TEST(MidasGradientTemperature, HotTopFaceCurlsCantileverDown) {
  const auto r = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":1,"TZ":20.0,"USE_HZ":true,"HZ":0.0,"TY":0.0,"USE_HY":true,"HY":0.0}]}})"));
  const auto tip = tipOf(r);
  const double kappa = 1.0e-5 * 20.0 / 1.0, L = 4.0;
  EXPECT_LT(tip.displacement[2], 0.0) << "hot top face must curl the tip DOWN";
  rel(tip.displacement[2], -kappa * L * L / 2, "tip w = -kappa L^2/2", 1e-9);
  EXPECT_NEAR(tip.displacement[1], 0.0, 1e-15);
}

TEST(MidasGradientTemperature, HotPlusYFaceMovesTipTowardMinusY) {
  const auto r = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":1,"TZ":0.0,"USE_HZ":true,"HZ":0.0,"TY":20.0,"USE_HY":true,"HY":0.0}]}})"));
  const auto tip = tipOf(r);
  const double kappa = 1.0e-5 * 20.0 / 0.5, L = 4.0;
  EXPECT_LT(tip.displacement[1], 0.0) << "hot +y face must move the tip toward -y";
  rel(tip.displacement[1], -kappa * L * L / 2, "tip v = -kappa L^2/2", 1e-9);
  EXPECT_NEAR(tip.displacement[2], 0.0, 1e-15);
}

TEST(MidasGradientTemperature, PlateTypeAndMissingDepthAreRejectedNotGuessed) {
  const auto plate = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":2,"TZ":20.0,"USE_HZ":true}]}})"));
  EXPECT_TRUE(hasMessage(plate.report.rejected, "GTMP element 1"));
  const auto noDepth = importMidasJson(model(kSB, R"(,"GTMP":{"1":{"ITEMS":[{"ID":1,"LCNAME":"TG","TYPE":1,"TZ":20.0,"USE_HZ":false,"HZ":0.0}]}})"));
  EXPECT_TRUE(hasMessage(noDepth.report.rejected, "GTMP element 1"));
  for (const auto& lc : noDepth.input.load_cases) EXPECT_TRUE(lc.thermal.empty());
}

TEST(MidasStructureType, XzPlaneRestrainsOutOfPlaneDofsEverywhere) {
  const auto r = importMidasJson(model(kSB, R"(,"STYP":{"1":{"STYP":1}})"));
  const auto& free = r.input.restraints.at(2);
  EXPECT_FALSE(free[0]); EXPECT_TRUE(free[1]); EXPECT_FALSE(free[2]);
  EXPECT_TRUE(free[3]); EXPECT_FALSE(free[4]); EXPECT_TRUE(free[5]);
  const auto threeD = importMidasJson(model(kSB, R"(,"STYP":{"1":{"STYP":0}})"));
  EXPECT_EQ(threeD.input.restraints.count(2), 0u);
  const auto other = importMidasJson(model(kSB, R"(,"STYP":{"1":{"STYP":2}})"));
  EXPECT_TRUE(hasMessage(other.report.rejected, "STYP=2"));
}
