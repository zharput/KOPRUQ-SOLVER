// 2026-11-20 -- catalog / database profiles removed by user decision (the solver targets precast
// concrete bridges first; steel members are entered as user-defined I/H or T sections). Every
// MIDAS section carrying a DB_NAME (AISC, EN/UNI, ...) must be rejected explicitly by the importer,
// never approximated, while the same shapes entered as user sections keep working. The SECT records
// below reproduce the exact shape of the catalog records in verification models 10 and 20.
// Dedicated new file.
#include <gtest/gtest.h>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

namespace {
nlohmann::json model(const std::string& sect) {
  return nlohmann::json::parse(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0},"3":{"X":0.0,"Y":1.0,"Z":1.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":)json" + sect + R"json(},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]},"3":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}}}})json");
}
std::string catalog(const char* shape, const char* db, const char* name) {
  return std::string(R"({"SECTTYPE":"DBUSER","SECT_NAME":"P","SECT_BEFORE":{"OFFSET_PT":"CC","USE_SHEAR_DEFORM":true,"SHAPE":")") + shape +
         R"(","SECT_I":{"DB_NAME":")" + db + R"(","SECT_NAME":")" + name + R"("}}})";
}
bool rejectedWith(const MidasImportResult& r, const std::string& text) {
  for (const auto& m : r.report.rejected) if (m.rfind("section 1: catalog/database section", 0) == 0 && m.find(text) != std::string::npos) return true;
  return false;
}
}

TEST(MidasCatalogRejected, AiscAndEnCatalogProfilesAreRejectedExplicitly) {
  const struct { const char* shape; const char* db; const char* name; } cases[] = {
      {"H", "AISC10(US)", "W36X247"}, {"H", "AISC10(US)", "HP14X89"}, // model 10
      {"H", "UNI", "HEB160"}, {"C", "UNI", "UPN100"}, {"L", "UNI", "L50x5"}, // model 20
  };
  for (const auto& c : cases) {
    const auto r = importMidasJson(model(catalog(c.shape, c.db, c.name)));
    EXPECT_TRUE(r.input.model.sections.empty()) << c.name;
    EXPECT_TRUE(rejectedWith(r, c.name)) << c.name;
  }
}

TEST(MidasCatalogRejected, UserDefinedIAndTSectionsStillImport) {
  const auto h = importMidasJson(model(R"({"SECTTYPE":"DBUSER","SECT_NAME":"I","SECT_BEFORE":{"SHAPE":"H","USE_SHEAR_DEFORM":true,"SECT_I":{"vSIZE":[1.0,0.5,0.02,0.03,0.6,0.05,0,0]}}})"));
  ASSERT_EQ(h.input.model.sections.size(), 1u);
  EXPECT_NEAR(h.input.model.sections[0].area, 0.0634, 1e-12);
  const auto t = importMidasJson(model(R"({"SECTTYPE":"DBUSER","SECT_NAME":"T","SECT_BEFORE":{"SHAPE":"T","USE_SHEAR_DEFORM":true,"SECT_I":{"vSIZE":[2.0,2.0,1.0,1.0]}}})"));
  ASSERT_EQ(t.input.model.sections.size(), 1u);
  EXPECT_NEAR(t.input.model.sections[0].J, 1.0528515625, 1e-12);
}
