// Phase 5.4 revision (2026-11-18) — MIDAS NX ELNK ("GEN" elastic link) import, verified against a
// REAL MIDAS export's own reference results. The DBG payload below is transcribed verbatim from
// verification/models/07 Basit Kiris Uniform yayli/07 Basit Kiris Uniform yayli.json (a real
// MIDAS NX export containing one non-coincident ELNK: node 10 at (0,0,-0.5) linked to node 1 at
// (0,0,0), SDR=[5000,5000,100000,0,0,0], ANGLE=0) -- embedded inline here (rather than read from
// that file, which is a local, untracked sample directory, not part of the repository) so this
// test is self-contained and runs in CI. The expected displacement/reaction values are transcribed
// from that model's own real MIDAS-computed Displacements(Global).xlsx / Reaction(Global).xlsx
// (not computed by this solver -- an independent reference, per this project's verification
// standard). Node 10 is fully restrained and connects to the rest of the structure ONLY through
// the ELNK spring (no beam touches it), so a correct match here is a real, non-trivial check of
// the ELNK -> SpringConnection import (orientation, stiffness mapping) -- not just "it doesn't
// throw".
#include <gtest/gtest.h>
#include "kopruq/io/midas/MidasJsonImporter.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"
#include <nlohmann/json.hpp>

namespace {
// clang-format off
const char* kModel07Json = R"JSON({
  "DBG": {
    "NODE": {
      "1": {"X": 0.0, "Y": 0.0, "Z": 0.0}, "2": {"X": 1.25, "Y": 0.0, "Z": 0.0},
      "3": {"X": 2.5, "Y": 0.0, "Z": 0.0}, "4": {"X": 3.75, "Y": 0.0, "Z": 0.0},
      "5": {"X": 5.0, "Y": 0.0, "Z": 0.0}, "6": {"X": 6.25, "Y": 0.0, "Z": 0.0},
      "7": {"X": 7.5, "Y": 0.0, "Z": 0.0}, "8": {"X": 8.75, "Y": 0.0, "Z": 0.0},
      "9": {"X": 10.0, "Y": 0.0, "Z": 0.0}, "10": {"X": 0.0, "Y": 0.0, "Z": -0.5}
    },
    "MATL": {"1": {"TYPE": "CONC", "NAME": "C35/45",
      "PARAM": [{"P_TYPE": 1, "STANDARD": "EN04(RC)", "CODE": "", "DB": "C35/45"}]}},
    "MATD": {"1": {"TYPE": "CONC", "NAME": "C35/45",
      "DATA1": {"CODENAME": "EN04(RC)", "CODEMATLNAME": "C35/45",
        "DESIGN": {"C_FC": 35000.0, "bLAMBDA": false, "C_FCI": 24500.0}}}},
    "SECT": {"1": {"SECTTYPE": "DBUSER", "SECT_NAME": "SUPER", "SECT_BEFORE": {
      "USE_SHEAR_DEFORM": true, "SHAPE": "SB", "DATATYPE": 2,
      "SECT_I": {"vSIZE": [0.3, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0]}}}},
    "CONS": {
      "9": {"ITEMS": [{"ID": 1, "CONSTRAINT": "1111000"}]},
      "10": {"ITEMS": [{"ID": 1, "CONSTRAINT": "1111111"}]}
    },
    "ELNK": {"1": {"LINK": "GEN", "NODE": [10, 1], "ANGLE": 0.0, "DR": [0.5, 0.5],
      "R_S": [false, false, false, false, false, false],
      "SDR": [5000.0, 5000.0, 100000.0, 0.0, 0.0, 0.0], "bSHEAR": false}},
    "ELEM": {
      "1": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [1, 2, 0,0,0,0,0,0], "ANGLE": 0.0},
      "2": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [2, 3, 0,0,0,0,0,0], "ANGLE": 0.0},
      "3": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [3, 4, 0,0,0,0,0,0], "ANGLE": 0.0},
      "4": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [4, 5, 0,0,0,0,0,0], "ANGLE": 0.0},
      "5": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [5, 6, 0,0,0,0,0,0], "ANGLE": 0.0},
      "6": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [6, 7, 0,0,0,0,0,0], "ANGLE": 0.0},
      "7": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [7, 8, 0,0,0,0,0,0], "ANGLE": 0.0},
      "8": {"TYPE": "BEAM", "MATL": 1, "SECT": 1, "NODE": [8, 9, 0,0,0,0,0,0], "ANGLE": 0.0}
    },
    "BMLD": {
      "1": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "2": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "3": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "4": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "5": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "6": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "7": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]},
      "8": {"ITEMS": [{"ID": 1, "LCNAME": "FORCE", "CMD": "BEAM", "TYPE": "UNILOAD", "DIRECTION": "GZ", "D": [0.0,1.0,0.0,0.0], "P": [-5.0,-5.0,0.0,0.0]}]}
    },
    "STLD": {"1": {"NO": 1, "NAME": "FORCE", "TYPE": "D"}}
  }
})JSON";
// clang-format on
}

TEST(MidasElasticLink, ImportsGenLinkAndSolvesMatchingRealMidasReference) {
  const auto parsed = nlohmann::json::parse(kModel07Json);
  const auto result = kopruq::io::midas::importMidasJson(parsed);
  ASSERT_EQ(result.report.elastic_links_rejected, 0u);
  ASSERT_EQ(result.report.springs, 1u);
  const auto& model = result.input;

  ASSERT_EQ(model.model.springs.size(), 1u);
  const auto& link = model.model.springs[0];
  EXPECT_EQ(link.node1, 10);
  EXPECT_EQ(link.node2, 1);
  // Non-coincident nodes along global Z: local_x must point node1->node2, i.e. +Z.
  EXPECT_NEAR(link.local_x.x(), 0.0, 1e-12);
  EXPECT_NEAR(link.local_x.y(), 0.0, 1e-12);
  EXPECT_NEAR(link.local_x.z(), 1.0, 1e-12);
  EXPECT_NEAR(link.stiffness[0], 5000.0, 1e-9);
  EXPECT_NEAR(link.stiffness[1], 5000.0, 1e-9);
  EXPECT_NEAR(link.stiffness[2], 100000.0, 1e-9);

  auto solution = kopruq::LinearStaticSolver::solveCases(model.model, model.restraints, model.load_cases, model.combinations);
  ASSERT_TRUE(solution.status.success) << solution.status.message;
  const auto& results = solution.cases.front().results;
  EXPECT_LT(results.force_equilibrium_error, 1e-6);
  EXPECT_LT(results.moment_equilibrium_error, 1e-6);

  // Independent reference values, transcribed from the real MIDAS export's own
  // Displacements(Global).xlsx / Reaction(Global).xlsx for load case "FORCE" (not computed by
  // this solver). Node 1 is free and connects to the structure only via beam 1 (to node 2) and
  // the ELNK spring (to node 10), so matching its displacement is a real check of the spring's
  // contribution, not a beam-only result.
  auto nodeResult = [&](kopruq::Id id) {
    for (const auto& n : results.nodes) if (n.node_id == id) return n;
    throw std::runtime_error("node not found");
  };
  // Tolerances match this project's own established displacement/rotation comparison defaults
  // (verification/ComparisonEngine.h's ToleranceSettings: 1e-4 m / 1e-5 rad), not tightened --
  // the material elastic modulus is itself an EN1992-1-1 table value, not MIDAS's exact fck-based
  // figure (see MidasMaterialLibrary.hpp), so sub-tolerance differences are expected, not a defect.
  const auto n1 = nodeResult(1);
  EXPECT_NEAR(n1.displacement[2], -0.004999999888241291, 1e-4) << "node 1 DZ";
  EXPECT_NEAR(n1.displacement[4], 0.0022171579767018557, 1e-5) << "node 1 RY";

  // Node 10 is fully restrained and carries no beam -- its entire reaction is transmitted purely
  // through the ELNK spring from node 1's side of the structure.
  const auto n10 = nodeResult(10);
  EXPECT_NEAR(n10.reaction[2], 25.0, 1e-4) << "node 10 FZ reaction (via ELNK only)";
  const auto n9 = nodeResult(9);
  EXPECT_NEAR(n9.reaction[2], 25.0, 1e-4) << "node 9 FZ reaction";
}
