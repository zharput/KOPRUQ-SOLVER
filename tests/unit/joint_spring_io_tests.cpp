// Phase 5 (2026-11-18) — JSON model input for the "joint_springs" array (kopruq-model v1).
// Numerical correctness is independently verified in joint_spring_tests.cpp; this file checks the
// reader wiring: successful parse, end-to-end solve parity with the direct C++ API, and rejection
// of invalid input.
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {
std::string baseJson(const char* jointSprings) {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0}],
    "materials":[], "sections":[], "beams":[],
    "joint_springs":)") + jointSprings + R"(,
    "supports":[{"node":1,"restraints":[false,true,true,true,true,true]}],
    "load_cases":[{"name":"c","nodal":[{"node":1,"values":[10,0,0,0,0,0]}]}]
  })";
}
}

TEST(JointSpringIo, ParsesAndSolvesMatchingDirectApi) {
  auto in = readModelText(baseJson(R"([{"id":1,"node":1,"local_x":[1,0,0],"reference_y":[0,1,0],"stiffness":[1000,0,0,0,0,0]}])"));
  ASSERT_EQ(in.model.joint_springs.size(), 1u);
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
  double ux = 0; for (const auto& n : r.cases.front().results.nodes) if (n.node_id == 1) ux = n.displacement[0];
  EXPECT_NEAR(ux, 10.0/1000.0, 1e-9);
}

TEST(JointSpringIo, DefaultsLocalXAndReferenceYWhenOmitted) {
  auto in = readModelText(baseJson(R"([{"id":1,"node":1,"stiffness":[1000,0,0,0,0,0]}])"));
  ASSERT_EQ(in.model.joint_springs.size(), 1u);
  EXPECT_DOUBLE_EQ(in.model.joint_springs[0].local_x.x(), 1.0);
  EXPECT_DOUBLE_EQ(in.model.joint_springs[0].reference_y.y(), 1.0);
}

TEST(JointSpringIo, RejectsNegativeStiffness) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"node":1,"stiffness":[-1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(JointSpringIo, RejectsUnknownNode) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"node":99,"stiffness":[1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(JointSpringIo, RejectsMissingStiffness) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"node":1}])")), ModelInputError);
}

TEST(JointSpringIo, RejectsDuplicateId) {
  EXPECT_THROW(readModelText(baseJson(
      R"([{"id":1,"node":1,"stiffness":[1,0,0,0,0,0]},{"id":1,"node":1,"stiffness":[2,0,0,0,0,0]}])")),
      ModelInputError);
}

TEST(JointSpringIo, RejectsParallelLocalXAndReferenceY) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"node":1,"local_x":[1,0,0],"reference_y":[2,0,0],"stiffness":[1,0,0,0,0,0]}])")), ModelInputError);
}

TEST(JointSpringIo, SpringAndJointSpringIdSpacesAreIndependent) {
  // A spring (two-node) and a joint spring (one-node) may share the same id value: id(1) used for
  // both is NOT a collision, since joint_springs is a separate id space (see Model.hpp).
  std::string text = R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":0,"y":0,"z":0}],
    "materials":[], "sections":[], "beams":[],
    "springs":[{"id":1,"nodes":[1,2],"stiffness":[1000,0,0,0,0,0]}],
    "joint_springs":[{"id":1,"node":1,"stiffness":[500,0,0,0,0,0]}],
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]},
                {"node":2,"restraints":[false,true,true,true,true,true]}]
  })";
  auto in = readModelText(text);
  EXPECT_EQ(in.model.springs.size(), 1u);
  EXPECT_EQ(in.model.joint_springs.size(), 1u);
}
