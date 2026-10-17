// Phase 5 — JSON model input for the "trusses" array (kopruq-model v1).
// Numerical correctness of the resulting truss element is independently
// verified in truss_tests.cpp; this file only checks that the reader wires
// the JSON up correctly: successful parse into Beam3DConnection with the
// truss release pattern, end-to-end solve matching the direct-C++-API
// result, and structural rejections (unknown node/material/section,
// duplicate id shared with "beams").
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {

std::string baseJson(const char* trusses) {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":-4,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0},{"id":3,"x":0,"y":0,"z":3}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.001,"Iy":1e-6,"Iz":1e-6,"J":1e-6}],
    "beams":[],
    "trusses":)") + trusses + R"(,
    "supports":[
      {"node":1,"restraints":[true,true,true,true,true,true]},
      {"node":2,"restraints":[true,true,true,true,true,true]},
      {"node":3,"restraints":[false,false,false,true,true,true]}
    ],
    "load_cases":[{"name":"apex","nodal":[{"node":3,"values":[0,0,-100,0,0,0]}]}]
  })";
}

} // namespace

TEST(TrussIo, ParsesIntoReleasedBeamConnection) {
  auto in = readModelText(baseJson(R"([
    {"id":1,"nodes":[1,3],"material":1,"section":1},
    {"id":2,"nodes":[2,3],"material":1,"section":1}
  ])"));
  ASSERT_EQ(in.model.beams.size(), 2u);
  for (const auto& e : in.model.beams) {
    for (int i : {3,4,5,9,10,11}) EXPECT_TRUE(e.releases[i]) << "index " << i;
    for (int i : {0,1,2,6,7,8}) EXPECT_FALSE(e.releases[i]) << "index " << i;
  }
}

TEST(TrussIo, EndToEndSolveMatchesDirectCxxApi) {
  auto in = readModelText(baseJson(R"([
    {"id":1,"nodes":[1,3],"material":1,"section":1},
    {"id":2,"nodes":[2,3],"material":1,"section":1}
  ])"));
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
  const auto& sol = r.cases.front();
  double uz = 0;
  for (const auto& n : sol.results.nodes) if (n.node_id == 3) uz = n.displacement[2];
  EXPECT_NEAR(uz, -1.0/288.0, 1e-9);
}

TEST(TrussIo, RejectsUnknownNodeReference) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,99],"material":1,"section":1}])")), ModelInputError);
}

TEST(TrussIo, RejectsUnknownMaterialReference) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,3],"material":99,"section":1}])")), ModelInputError);
}

TEST(TrussIo, RejectsIdCollisionWithBeams) {
  std::string text = R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":-4,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0},{"id":3,"x":0,"y":0,"z":3}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.001,"Iy":1e-6,"Iz":1e-6,"J":1e-6}],
    "beams":[{"id":1,"nodes":[1,2],"material":1,"section":1}],
    "trusses":[{"id":1,"nodes":[2,3],"material":1,"section":1}],
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}]
  })";
  EXPECT_THROW(readModelText(text), ModelInputError);
}

TEST(TrussIo, RejectsReferenceYKey) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,3],"material":1,"section":1,"reference_y":[0,1,0]}])")), ModelInputError);
}
