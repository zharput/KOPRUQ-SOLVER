// 2026-11-23 -- log-log response-spectrum interpolation (SpectrumInterpolation::LogLog; MIDAS SPLC
// INTERP "LOG", identified from MIDAS's own results of verification models 100 and 16).
// Dedicated new file. Expected values are closed forms evaluated here by hand formulas.
#include <gtest/gtest.h>
#include <cmath>
#include <string>
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using kopruq::ResponseSpectrumPoint;
using kopruq::SpectrumInterpolation;

TEST(SpectrumLogLog, PowerLawSpectrumIsReproducedExactly) {
  // Sa = C / T (Eurocode 8 constant-velocity branch): log-log interpolation is exact for a power law.
  const double C = 1.2;
  const std::vector<ResponseSpectrumPoint> sp = {{0.5, C / 0.5}, {2.0, C / 2.0}};
  for (double T : {0.6, 0.9, 1.37, 1.9})
    EXPECT_NEAR(kopruq::interpolateSpectrum(sp, T, SpectrumInterpolation::LogLog), C / T, 1e-12 * C / T) << T;
  // Linear interpolation is not (chord above the convex curve) -- and stays the default.
  EXPECT_GT(kopruq::interpolateSpectrum(sp, 1.0), C / 1.0 + 0.1);
  // Table points are returned exactly in both modes.
  EXPECT_DOUBLE_EQ(kopruq::interpolateSpectrum(sp, 2.0, SpectrumInterpolation::LogLog), C / 2.0);
}

TEST(SpectrumLogLog, ZeroOrdinateIsRejected) {
  const std::vector<ResponseSpectrumPoint> sp = {{0.1, 0.0}, {1.0, 2.0}};
  EXPECT_THROW(kopruq::interpolateSpectrum(sp, 0.5, SpectrumInterpolation::LogLog), std::invalid_argument);
  EXPECT_NO_THROW(kopruq::interpolateSpectrum(sp, 0.5, SpectrumInterpolation::Linear));
}

TEST(SpectrumLogLog, SingleModeResponseScalesWithTheInterpolatedSa) {
  // Cantilever, one mode requested: every peak response is |Gamma Sa / omega^2 phi| (SRSS of one
  // mode), so the log-log / linear ratio of any response equals the ratio of the two Sa values at
  // the mode period, computed below by hand from the two table points.
  kopruq::Model m;
  m.nodes = {{1, 0, 0, 0}, {2, 0, 0, 4.0}};
  m.materials = {{1, 3e7, 0.2, 25}};
  m.sections = {{1, 0.5, 0.02, 0.02, 0.03}};
  m.beams = {{1, 1, 2, 1, 1, {0, 1, 0}}};
  const std::map<kopruq::Id, std::array<bool,6>> rs = {{1, {true, true, true, true, true, true}}};
  auto modal = kopruq::ModalSolver::solve(m, rs, 1);
  ASSERT_TRUE(modal.status.success);
  const double T = 1.0 / modal.modes[0].frequency_hz;
  const double t0 = 0.5 * T, t1 = 3.0 * T, a0 = 6.0, a1 = 1.5;
  kopruq::ResponseSpectrumInput in; in.direction = 0; in.num_modes = 1; in.spectrum = {{t0, a0}, {t1, a1}};
  auto lin = kopruq::ResponseSpectrumSolver::solve(m, rs, in);
  in.interpolation = SpectrumInterpolation::LogLog;
  auto lgr = kopruq::ResponseSpectrumSolver::solve(m, rs, in);
  ASSERT_TRUE(lin.status.success && lgr.status.success);
  const double saLin = a0 + (T - t0) / (t1 - t0) * (a1 - a0);
  const double saLog = std::exp(std::log(a0) + (std::log(T) - std::log(t0)) / (std::log(t1) - std::log(t0)) * (std::log(a1) - std::log(a0)));
  const auto& nl = lin.combined_nodes[1]; const auto& ng = lgr.combined_nodes[1];
  ASSERT_GT(std::abs(nl.displacement[0]), 0.0);
  EXPECT_NEAR(ng.displacement[0] / nl.displacement[0], saLog / saLin, 1e-12);
  EXPECT_NEAR(lgr.combined_nodes[0].reaction[0] / lin.combined_nodes[0].reaction[0], saLog / saLin, 1e-12);
}

namespace {
nlohmann::json midasModel(const char* interp) {
  return nlohmann::json::parse(std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":0.0,"Y":0.0,"Z":4.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[0.5,0.5]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]}},
    "SPFC":{"1":{"NAME":"F","GRAV":9.806,"DRATIO":0.05,"aFUNC":[{"PERIOD":0.0,"VALUE":0.1},{"PERIOD":0.5,"VALUE":0.25},{"PERIOD":5.0,"VALUE":0.025}]}},
    "SPLC":{"1":{"NAME":"SPX","DIR":"XY","ANGLE":0.0,"COMTYPE":"CQC","INTERP":")json") + interp + R"json(","aFUNCNAME":["F"]}}}})json");
}
}

TEST(SpectrumLogLog, MidasInterpMapping) {
  using namespace kopruq::io::midas;
  const auto lg = importMidasJson(midasModel("LOG"));
  ASSERT_EQ(lg.input.response_spectra.size(), 1u);
  EXPECT_EQ(lg.input.response_spectra[0].input.interpolation, SpectrumInterpolation::LogLog);
  EXPECT_DOUBLE_EQ(lg.input.response_spectra[0].input.spectrum.front().period_s, 1e-6); // T = 0 stand-in
  bool noted = false; for (const auto& a : lg.report.assumptions) if (a.find("INTERP=LOG") != std::string::npos) noted = true;
  EXPECT_TRUE(noted);
  const auto li = importMidasJson(midasModel("LINEAR"));
  ASSERT_EQ(li.input.response_spectra.size(), 1u);
  EXPECT_EQ(li.input.response_spectra[0].input.interpolation, SpectrumInterpolation::Linear);
  const auto bad = importMidasJson(midasModel("SPLINE"));
  EXPECT_TRUE(bad.input.response_spectra.empty());
}

TEST(SpectrumLogLog, NativeJsonRoundTrip) {
  using namespace kopruq::io;
  auto in = midas::importMidasJson(midasModel("LOG")).input;
  auto j = writeModel(in);
  EXPECT_EQ(j["response_spectra"][0]["interpolation"], "loglog");
  EXPECT_EQ(readModel(j).response_spectra.at(0).input.interpolation, SpectrumInterpolation::LogLog);
  auto lin = midas::importMidasJson(midasModel("LINEAR")).input;
  EXPECT_FALSE(writeModel(lin)["response_spectra"][0].contains("interpolation")); // default omitted
  j["response_spectra"][0]["interpolation"] = "cubic";
  EXPECT_THROW(readModel(j), ModelInputError);
}
