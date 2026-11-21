// 2026-11-21 -- MIDAS NX PSC "PSCI" (PSC-I precast girder) section, verification model 24.
// Dedicated new file. Expected geometric literals: independent exact rational polygon integration
// (Python fractions, outside this code base) of the manual's PSC-I outline with the joint offsets
// BL2-1 / BL4-1 measured from the flange edges. MIDAS values: its own section table / model file
// (model 24): GIRDER-1 A 0.62, Iyy 0.206145519713262, Izz 0.0229666666666667, J 0.0133109207412865,
// Asy 0.385168913009211, Asz 0.303491944898104; GIRDER-2 A 1.513, Iyy 1.28533493490123,
// Izz 0.177406666666667, J 0.0674516953696603, Asy 0.83836890708866, Asz 0.719058002436261.
// J / Asy / Asz come from MIDAS's own (undocumented) section mesh; KOPRUQ's converged FE values sit
// slightly below them, as for PSC 1CEL (docs/DEVELOPMENT_STATUS.md): checked within 0.3 %.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol) {
  EXPECT_NEAR(actual, expected, tol * std::abs(expected)) << what;
}
const PscIDims kGirder1{0.1, 0.1, 0.0, 1.0, 0.2, 0.0, 0.2, 0.1, 0.5, 0.0, 0.4, 0.0, false, false};
const PscIDims kGirder2{0.15, 0.25, 0.03, 1.5, 0.4, 0.1, 0.2, 0.15, 1.0, 0.3, 0.6, 0.15, true, true};
}

TEST(MidasPscI, Girder1GeometryExact) {
  const auto p = pscISection(kGirder1);
  rel(p.area, 0.62, "A", 1e-12);
  rel(p.IH, 0.20614551971326164, "Iyy (independent)", 1e-11);
  rel(p.IB, 0.022966666666666666, "Izz (independent)", 1e-11);
  rel(p.topFiber, 0.84731182795698923, "centroid below top", 1e-11);
  rel(p.IH, 0.206145519713262, "Iyy vs MIDAS", 1e-11);
  rel(p.extentH, 1.6, "height", 1e-15); rel(p.extentB, 1.0, "width", 1e-15);
}

TEST(MidasPscI, Girder2WithJointsGeometryExact) {
  const auto p = pscISection(kGirder2);
  rel(p.area, 1.513, "A", 1e-12);
  rel(p.IH, 1.2853349349012264, "Iyy (independent)", 1e-11);
  rel(p.IB, 0.17740666666666666, "Izz (independent)", 1e-11);
  rel(p.topFiber, 1.179222295659837, "centroid below top", 1e-11);
  rel(p.IH, 1.28533493490123, "Iyy vs MIDAS", 1e-11);
  rel(p.IB, 0.177406666666667, "Izz vs MIDAS", 1e-11);
}

TEST(MidasPscI, MeshDependentPropertiesCloseToMidas) {
  const auto g1 = pscISection(kGirder1), g2 = pscISection(kGirder2);
  rel(g1.J, 0.0133109207412865, "G1 J", 3e-3);   rel(g2.J, 0.0674516953696603, "G2 J", 3e-3);
  rel(g1.AsB, 0.385168913009211, "G1 Asy", 3e-3); rel(g2.AsB, 0.83836890708866, "G2 Asy", 3e-3);
  rel(g1.AsH, 0.303491944898104, "G1 Asz", 3e-3); rel(g2.AsH, 0.719058002436261, "G2 Asz", 3e-3);
  // Converged FE lies below MIDAS's coarser-mesh values (same direction as PSC 1CEL).
  EXPECT_LT(g1.J, 0.0133109207412865); EXPECT_LT(g2.J, 0.0674516953696603);
}

TEST(MidasPscI, InvalidJointsRejected) {
  PscIDims bad = kGirder2; bad.BL21 = 0.9; // JL1 inside the web
  EXPECT_THROW(pscISection(bad), std::invalid_argument);
  bad = kGirder2; bad.HL41 = 0.5;          // JL4 above the web bottom
  EXPECT_THROW(pscISection(bad), std::invalid_argument);
}

namespace {
nlohmann::json model(const std::string& joints, const std::string& a = "[0.0,0.15,0.25,0.03,0.0,1.5,0.4,0.1,0.0,0.2]") {
  return nlohmann::json::parse(std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":10.0,"Y":0.0,"Z":0.0},"3":{"X":0.0,"Y":1.0,"Z":1.0}},
    "MATL":{"1":{"TYPE":"CONC","NAME":"C35/45","PARAM":[{"P_TYPE":1,"STANDARD":"EN04(RC)","CODE":"","DB":"C35/45"}]}},
    "SECT":{"3":{"SECTTYPE":"PSC","SECT_NAME":"GIRDER-2","SECT_BEFORE":{"OFFSET_PT":"CT","USE_SHEAR_DEFORM":true,"SHAPE":"PSCI",
      "SECT_I":{"vSIZE_PSC_A":)json") + a + R"json(,"vSIZE_PSC_B":[0.15,1.0,0.3,0.0,0.6,0.15,0.0],
                "vSIZE_PSC_C":[0.15,0.25,0.03,0.0,1.5,0.4,0.1,0.0,0.2],"vSIZE_PSC_D":[0.15,1.0,0.3,0.0,0.6,0.15,0.0]},
      "USE_SYMMETRIC":true,"USE_SMALL_HOLE":false,"USE_USER_DEF_MESHSIZE":false,"USE_USER_INTPUT_STIFF":false,"JOINT":)json" + joints + R"json(}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":3,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]},"3":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}}}})json");
}
bool rejectedWith(const MidasImportResult& r, const char* text) {
  for (const auto& m : r.report.rejected) if (m.find(text) != std::string::npos) return true;
  return false;
}
}

TEST(MidasPscI, ImporterMapsModel24GirderAndRejectsUnverifiedVariants) {
  const auto ok = importMidasJson(model("[false,true,false,false,true,true,false,false,true]"));
  ASSERT_EQ(ok.input.model.sections.size(), 1u);
  rel(ok.input.model.sections[0].area, 1.513, "A", 1e-12);
  rel(ok.input.model.sections[0].Iy, 1.2853349349012264, "Iy", 1e-11);
  ASSERT_EQ(ok.input.model.beams.size(), 1u);
  rel(-ok.input.model.beams[0].offset1.z(), 1.179222295659837, "CT offset = centroid below top", 1e-11);
  EXPECT_TRUE(rejectedWith(importMidasJson(model("[true,true,false,false,true,true,false,false,true]")), "J1"));
  EXPECT_TRUE(rejectedWith(importMidasJson(model("[false,true,true,false,true,true,true,false,true]")), "JL2"));
  EXPECT_TRUE(rejectedWith(importMidasJson(model("[false,true,false,false,true,false,false,false,true]")), "different left/right joints"));
  EXPECT_TRUE(rejectedWith(importMidasJson(model("[false,true,false,false,true,true,false,false,true]", "[0.1,0.15,0.25,0.03,0.0,1.5,0.4,0.1,0.0,0.2]")), "H1"));
}
