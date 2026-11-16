// Phase 7B requalification (2026-11-16) — end-to-end check that MidasJsonImporter actually
// wires computed shear areas onto the imported Section when USE_SHEAR_DEFORM is true, and
// leaves Asy=Asz=0 (Euler-Bernoulli) when it is false. Self-contained inline JSON, independent
// of xmidas/deneme1.json (which is not present in this working tree).
#include <gtest/gtest.h>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

namespace {
nlohmann::json minimalModel(bool useShearDeform) {
  return nlohmann::json::parse(R"JSON({
    "DBG": {
      "NODE": {"1": {"X":0.0,"Y":0.0,"Z":0.0}, "2": {"X":1.0,"Y":0.0,"Z":0.0}},
      "ELEM": {"1": {"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0,"STYPE":0}},
      "CONS": {"1": {"ITEMS":[{"ID":1,"GROUP_NAME":"","CONSTRAINT":"111111"}]}},
      "MATL": {"1": {"TYPE":"CONC","NAME":"C35/45","HE_SPEC":0.0,"HE_COND":0.0,"bMASS_DENS":false,"DAMP_RAT":0.05,
        "PARAM":[{"P_TYPE":1,"STANDARD":"EN04(RC)","CODE":"","DB":"C35/45"}]}},
      "MATD": {"1": {"TYPE":"CONC","NAME":"C35/45","DATA1":{"CODENAME":"EN04(RC)","CODEMATLNAME":"C35/45",
        "DESIGN":{"C_FC":35000.0,"bLAMBDA":false,"C_FCI":24500.0}},"REBAR_CODENAME":"",
        "MAINREBAR_REBARNAME":"","SUBREBAR_REBARNAME":"","MAINREBAR_B_FY":0.0,"SUBREBAR_B_FY":0.0}},
      "SECT": {"1": {"SECTTYPE":"DBUSER","SECT_NAME":"SUPER","SECT_BEFORE":{"OFFSET_PT":"CT",
        "OFFSET_CENTER":0,"USER_OFFSET_REF":0,"HORZ_OFFSET_OPT":0,"USERDEF_OFFSET_YI":0.0,
        "VERT_OFFSET_OPT":0,"USERDEF_OFFSET_ZI":0.0,"USE_SHEAR_DEFORM":)JSON" + std::string(useShearDeform ? "true" : "false") + R"JSON(,
        "USE_WARPING_EFFECT":false,"SHAPE":"SB","DATATYPE":2,
        "SECT_I":{"vSIZE":[0.3,1.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0]}}}},
      "CNLD": {"2": {"ITEMS":[{"ID":1,"LCNAME":"FORCE","GROUP_NAME":"","FX":0.0,"FY":10.0,"FZ":0.0,"MX":0.0,"MY":0.0,"MZ":0.0}]}}
    }
  })JSON");
}
}

TEST(MidasJsonImporterShear, UseShearDeformTrueProducesNonzeroAsyAsz) {
  auto result = importMidasJson(minimalModel(true));
  ASSERT_EQ(result.input.model.sections.size(), 1u);
  const auto& sec = result.input.model.sections[0];
  const double area = 0.3 * 1.0;
  EXPECT_NEAR(sec.Asy, (5.0/6.0) * area, 1e-9); // Asy <- AsB (y-bending plane, uses Iz=IB)
  EXPECT_NEAR(sec.Asz, (5.0/6.0) * area, 1e-9); // Asz <- AsH (z-bending plane, uses Iy=IH)
  bool foundAssumption = false;
  for (const auto& a : result.report.assumptions)
    if (a.find("shear areas computed") != std::string::npos) foundAssumption = true;
  EXPECT_TRUE(foundAssumption);
}

TEST(MidasJsonImporterShear, UseShearDeformFalseLeavesAsyAszZero) {
  auto result = importMidasJson(minimalModel(false));
  ASSERT_EQ(result.input.model.sections.size(), 1u);
  const auto& sec = result.input.model.sections[0];
  EXPECT_EQ(sec.Asy, 0.0);
  EXPECT_EQ(sec.Asz, 0.0);
}
