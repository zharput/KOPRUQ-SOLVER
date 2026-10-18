// Phase 5 (deferred item) — JSON model input for the "rigid_links" array.
// Numerical correctness is independently verified in rigid_link_tests.cpp;
// this file checks the reader wiring and input rejections.
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {
std::string baseJson(const char* links, const char* slaveRestraint = "") {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0},{"id":3,"x":5.5,"y":0,"z":0}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
    "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
    "rigid_links":)") + links + R"(,
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]})" + slaveRestraint + R"(],
    "load_cases":[{"name":"c","nodal":[{"node":3,"values":[0,0,-1000,0,0,0]}]}]
  })";
}
}

TEST(RigidLinkIo, ParsesAndSolvesSuccessfully) {
  auto in = readModelText(baseJson(R"([{"id":1,"master":2,"slave":3}])"));
  ASSERT_EQ(in.model.rigid_links.size(), 1u);
  EXPECT_EQ(in.model.rigid_links[0].master, 2u);
  EXPECT_EQ(in.model.rigid_links[0].slave, 3u);
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
}

TEST(RigidLinkIo, RejectsUnknownMasterOrSlave) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":99,"slave":3}])")), ModelInputError);
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":2,"slave":99}])")), ModelInputError);
}

TEST(RigidLinkIo, RejectsSameMasterAndSlave) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":2,"slave":2}])")), ModelInputError);
}

TEST(RigidLinkIo, RejectsDuplicateLinkId) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":1,"slave":2},{"id":1,"master":2,"slave":3}])")), ModelInputError);
}

TEST(RigidLinkIo, RejectsSlaveWithTwoMasters) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":1,"slave":3},{"id":2,"master":2,"slave":3}])")), ModelInputError);
}

TEST(RigidLinkIo, RejectsChaining) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":1,"slave":2},{"id":2,"master":2,"slave":3}])")), ModelInputError);
}

TEST(RigidLinkIo, RejectsRestraintOnSlaveNode) {
  EXPECT_THROW(readModelText(baseJson(R"([{"id":1,"master":2,"slave":3}])", R"(,{"node":3,"restraints":[true,false,false,false,false,false]})")), ModelInputError);
}
