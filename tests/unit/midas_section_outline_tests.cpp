// 2026-11-23 -- section outlines for the model view (MidasSectionOutline.hpp). Dedicated new file.
// The outlines must describe the same section the solver uses: their area (exact polygon
// integration) equals the section library's own closed-form area (polygonal shapes exactly; circular
// parts within the polygon approximation; H fillets are not drawn, so the area is smaller by exactly
// the four spandrels (4 - pi) r1^2); after centring, the centroid is at the origin and the top fiber
// equals ResolvedSectionProperties::topFiber (the value the "CT" offset uses).
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <string>
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using namespace kopruq::io::midas;

namespace {
double outlineArea(const SectionOutline& o) { return kopruq::sections::computePolygonSection(o).area; }
double maxZ(const SectionOutline& o) { double z = -1e300; for (const auto& c : o) for (const auto& p : c) z = std::max(z, p.y()); return z; }
double minZ(const SectionOutline& o) { double z = 1e300; for (const auto& c : o) for (const auto& p : c) z = std::min(z, p.y()); return z; }
void checkShape(const std::string& shape, const std::vector<double>& v, double areaTol, double extentH) {
  const auto p = resolveDbUserSection(shape, v);
  const auto raw = dbUserSectionOutline(shape, v);
  EXPECT_NEAR(outlineArea(raw), p.area, areaTol * p.area) << shape << " area";
  const auto c = centroidalOutline(raw);
  const auto g = kopruq::sections::computePolygonSection(c);
  EXPECT_NEAR(g.Cy, 0.0, 1e-12) << shape; EXPECT_NEAR(g.Cz, 0.0, 1e-12) << shape;
  if (extentH > 0) {
    EXPECT_NEAR(maxZ(c), p.topFiber, 2e-3 * extentH) << shape << " top fiber";
    EXPECT_NEAR(maxZ(c) - minZ(c), extentH, 1e-12 * extentH) << shape << " depth";
  }
}
}

TEST(MidasSectionOutline, PolygonalShapesMatchSectionAreaExactly) {
  checkShape("SB", {1.3, 3.0}, 1e-12, 1.3);
  checkShape("B", {2.0, 5.0, 0.5, 0.5}, 1e-12, 2.0);
  checkShape("T", {2.0, 2.0, 1.0, 1.0}, 1e-12, 2.0);
  checkShape("H", {1.0, 0.5, 0.02, 0.03, 0.6, 0.05, 0.0, 0.0}, 1e-12, 1.0);
  checkShape("H", {2.0, 2.0, 1.0, 0.5, 0.0, 0.0, 0.0, 0.0}, 1e-12, 2.0); // B2 = tf2 = 0 -> symmetric
  checkShape("C", {0.1, 0.05, 0.006, 0.0085, 0.05, 0.0085, 0.0, 0.0}, 1e-12, 0.1);
  checkShape("L", {0.05, 0.05, 0.005, 0.005}, 1e-12, 0.0); // truss-only section: no topFiber/extent
  checkShape("SOCT", {2.5, 1.3, 0.3, 0.4}, 1e-12, 2.5);
}

TEST(MidasSectionOutline, CircularShapesWithinPolygonApproximation) {
  // A regular 48-gon inscribed in the circle has area (48/2) sin(2 pi/48) r^2 = 0.99715 pi r^2.
  checkShape("SR", {1.2}, 3e-3, 1.2);
  checkShape("STRK", {1.0, 2.5}, 3e-3, 1.0);
}

TEST(MidasSectionOutline, FilletsAreNotDrawn) {
  const std::vector<double> v{0.16, 0.16, 0.008, 0.013, 0.0, 0.0, 0.015, 0.0};
  const auto p = resolveDbUserSection("H", v);
  const double r = 0.015;
  EXPECT_NEAR(outlineArea(dbUserSectionOutline("H", v)), p.area - (4 - std::acos(-1.0)) * r * r, 1e-12);
}

TEST(MidasSectionOutline, PscOutlinesAreTheSolverGeometry) {
  const Psc1CellDims box{0.25, 0.4, 2.5, 2.0, 0.0, 3.0, 0.25, 0.4, 2.0, 0.2, 0.3, 2.2, 2.2};
  const auto pb = psc1CellSection(box);
  const auto ob = centroidalOutline(psc1CellContours(box));
  ASSERT_EQ(ob.size(), 2u); // outer + cell
  EXPECT_NEAR(outlineArea(ob), pb.area, 1e-12 * pb.area);
  EXPECT_NEAR(maxZ(ob), pb.topFiber, 1e-12);
  const PscIDims girder{0.15, 0.25, 0.03, 1.5, 0.4, 0.1, 0.2, 0.15, 1.0, 0.3, 0.6, 0.15, true, true};
  const auto pi = pscISection(girder);
  const auto oi = centroidalOutline(pscIContours(girder));
  EXPECT_NEAR(outlineArea(oi), 1.513, 1e-12);
  EXPECT_NEAR(maxZ(oi), pi.topFiber, 1e-12);
}

TEST(MidasSectionOutline, ImporterFillsSectionOutlines) {
  const auto j = nlohmann::json::parse(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":0.0,"Y":0.0,"Z":5.0},"3":{"X":4.0,"Y":0.0,"Z":5.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}},
            "2":{"SECTTYPE":"DBUSER","SECT_NAME":"P","SECT_BEFORE":{"SHAPE":"SR","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.2]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":2,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0},
            "2":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[2,3,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}}}})json");
  const auto r = importMidasJson(j);
  ASSERT_EQ(r.input.section_outlines.size(), 2u);
  EXPECT_EQ(r.input.section_outlines.at(1).size(), 1u);
  EXPECT_EQ(r.input.section_outlines.at(2).front().size(), 48u);
  // The native model JSON does not carry outlines (display data only).
  EXPECT_FALSE(kopruq::io::writeModel(r.input).contains("section_outlines"));
}
