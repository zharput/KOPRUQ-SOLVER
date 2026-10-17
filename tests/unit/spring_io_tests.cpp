// Phase 5 — JSON model input for the "springs" array (kopruq-model v1).
// Numerical correctness is independently verified in spring_tests.cpp; this
// file checks the reader wiring: successful parse, end-to-end solve parity
// with the direct C++ API, and rejection of invalid input.
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {
std::string baseJson(const char* springs) {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":1,"y":0,"z":0}],
    "materials":[], "sections":[], "beams":[],
    "springs":)") + springs + R"(,
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]},
                {"node":2,"restraints":[false,true,true,true,true,true]}],
    "load_cases":[{"name":"c","nodal":[{"node":2,"values":[10,0,0,0,0,0]}]}]
  })";
}
}

TEST(SpringIo, ParsesAndSolvesMatchingDirectApi) {
  auto in = readModelText(baseJson(R"([{"id":1,"nodes":[1,2],"local_x":[1,0,0],"reference_y":[0,1,0],"stiffness":[1000,0,0,0,0,0]}])"));
  ASSERT_EQ(in.model.springs.size(), 1u);
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
  double ux = 0; for (const auto& n : r.cases.front().results.nodes) if (n.node_id == 2) ux = n.displacement[0];
  EXPECT_NEAR(ux, 10.0/1000.0, 1e-9);
}

TEST(SpringIo, DefaultsLocalXAndReferenceYWhenOmitted) {
  auto in = readModelText(baseJson(R"([{"id":1,"nodes":[1,2],"stiffness":[1000,0,0,0,0,0]}])"));
  ASSERT_EQ(in.model.springs.size(), 1u);
  EXPECT_DOUBLE_EQ(in.model.springs[0].local_x.x(), 1.0);
  EXPECT_DOUBLE_EQ(in.model.springs[0].reference_y.y(), 1.0);
}

TEST(SpringIo, RejectsNegativeStiffness) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,2],"stiffness":[-1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(SpringIo, RejectsUnknownNode) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,99],"stiffness":[1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(SpringIo, RejectsSameNodeTwiceForNodes) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,1],"stiffness":[1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(SpringIo, RejectsMissingStiffness) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,2]}])")), ModelInputError);
}

TEST(SpringIo, RejectsParallelLocalXAndReferenceY) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"nodes":[1,2],"local_x":[1,0,0],"reference_y":[2,0,0],"stiffness":[1,0,0,0,0,0]}])")), ModelInputError);
}
