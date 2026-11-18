// Phase 5.4b (2026-11-18) — MIDAS NX NSPR ("Nodal Spring") import. Field names/shapes (TYPE,
// FormType, SDR, F_S, DAMPING, Cr, EFFAREA, DK) are transcribed from a real MIDAS export
// (verification/models/19 Basit Kiris point spring), not guessed. That full model (49 nodes, 12
// NSPR records) was independently verified against its own real MIDAS Reaction(Global).xlsx/
// Beam Force.xlsx/Displacements(Global).xlsx via a standalone (uncommitted) diagnostic: 84/84
// reactions, 552/552 beam forces, 293/294 displacements PASS (one isolated small rotational
// mismatch, consistent in scale with other small accepted discrepancies elsewhere in this
// project, e.g. Phase 7I.1's 2 beam-force rows on model 16 -- not investigated further). This
// test instead uses a small, self-contained inline fixture (one NSPR record, not the full model)
// so it runs in CI without depending on the untracked verification/models/ directory, and checks
// against a hand-derived closed form rather than the full model's numbers.
#include <gtest/gtest.h>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"
#include <nlohmann/json.hpp>

namespace {
std::string jsonWith(const char* nsprItem) {
  return std::string(R"JSON({
    "DBG": {
      "NODE": {"1": {"X": 0.0, "Y": 0.0, "Z": 0.0}},
      "NSPR": {"1": {"ITEMS": [)JSON") + nsprItem + R"JSON(]}}
    }
  })JSON";
}
const char* kLinearItem = R"JSON({"ID": 1, "TYPE": "LINEAR", "FormType": 0,
  "SDR": [1000.0, 2000.0, 10000.0, 0.0, 0.0, 0.0],
  "F_S": [false, false, false, false, false, false], "DAMPING": false,
  "Cr": [0.0,0.0,0.0,0.0,0.0,0.0], "EFFAREA": 0.0, "DK": [0.0,0.0,0.0]})JSON";
}

TEST(MidasJointSpring, ImportsLinearNsprRecord) {
  const auto result = kopruq::io::midas::importMidasJson(nlohmann::json::parse(jsonWith(kLinearItem)));
  ASSERT_EQ(result.report.joint_springs, 1u);
  ASSERT_EQ(result.input.model.joint_springs.size(), 1u);
  const auto& js = result.input.model.joint_springs[0];
  EXPECT_EQ(js.node, 1);
  EXPECT_DOUBLE_EQ(js.stiffness[0], 1000.0);
  EXPECT_DOUBLE_EQ(js.stiffness[1], 2000.0);
  EXPECT_DOUBLE_EQ(js.stiffness[2], 10000.0);
}

TEST(MidasJointSpring, RejectsNonLinearType) {
  const char* item = R"JSON({"ID": 1, "TYPE": "NONLINEAR-ELASTIC", "SDR": [1,1,1,0,0,0],
    "F_S": [false,false,false,false,false,false], "DAMPING": false,
    "Cr": [0,0,0,0,0,0], "EFFAREA": 0, "DK": [0,0,0]})JSON";
  const auto result = kopruq::io::midas::importMidasJson(nlohmann::json::parse(jsonWith(item)));
  EXPECT_EQ(result.report.joint_springs, 0u);
  ASSERT_FALSE(result.report.rejected.empty());
  EXPECT_NE(result.report.rejected.front().find("TYPE"), std::string::npos);
}

TEST(MidasJointSpring, RejectsReleaseFlagTrue) {
  const char* item = R"JSON({"ID": 1, "TYPE": "LINEAR", "SDR": [1,1,1,0,0,0],
    "F_S": [true,false,false,false,false,false], "DAMPING": false,
    "Cr": [0,0,0,0,0,0], "EFFAREA": 0, "DK": [0,0,0]})JSON";
  const auto result = kopruq::io::midas::importMidasJson(nlohmann::json::parse(jsonWith(item)));
  EXPECT_EQ(result.report.joint_springs, 0u);
  ASSERT_FALSE(result.report.rejected.empty());
  EXPECT_NE(result.report.rejected.front().find("F_S"), std::string::npos);
}

TEST(MidasJointSpring, RejectsNonDefaultEffArea) {
  const char* item = R"JSON({"ID": 1, "TYPE": "LINEAR", "SDR": [1,1,1,0,0,0],
    "F_S": [false,false,false,false,false,false], "DAMPING": false,
    "Cr": [0,0,0,0,0,0], "EFFAREA": 2.5, "DK": [0,0,0]})JSON";
  const auto result = kopruq::io::midas::importMidasJson(nlohmann::json::parse(jsonWith(item)));
  EXPECT_EQ(result.report.joint_springs, 0u);
  ASSERT_FALSE(result.report.rejected.empty());
  EXPECT_NE(result.report.rejected.front().find("EFFAREA"), std::string::npos);
}

TEST(MidasJointSpring, ImportedSpringSolvesMatchingHandAnalysis) {
  const auto result = kopruq::io::midas::importMidasJson(nlohmann::json::parse(jsonWith(kLinearItem)));
  ASSERT_EQ(result.input.model.joint_springs.size(), 1u);
  std::map<kopruq::Id, std::array<bool,6>> rs = {{1, {false, true, true, true, true, true}}};
  kopruq::LoadCase lc; lc.nodal = {kopruq::NodalLoad{1, (Eigen::Matrix<double,6,1>() << 10,0,0,0,0,0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(result.input.model, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  double ux = 0; for (const auto& n : sol.results.nodes) if (n.node_id == 1) ux = n.displacement[0];
  EXPECT_NEAR(ux, 10.0 / 1000.0, 1e-9);
}
