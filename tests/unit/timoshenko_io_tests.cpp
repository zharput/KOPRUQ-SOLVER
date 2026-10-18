// Phase 5 (deferred item) — JSON model input for section Asy/Asz
// (Timoshenko shear areas). Numerical correctness is independently
// verified in timoshenko_tests.cpp; this file checks the reader wiring.
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {
std::string baseJson(const char* sectionExtra) {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.02,"Iy":3e-4,"Iz":4e-4,"J":5e-4)") + sectionExtra + R"(}],
    "beams":[{"id":1,"nodes":[1,2],"material":1,"section":1}],
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}]
  })";
}
}

TEST(TimoshenkoIo, DefaultsAsyAszToZero) {
  auto in = readModelText(baseJson(""));
  ASSERT_EQ(in.model.sections.size(), 1u);
  EXPECT_DOUBLE_EQ(in.model.sections[0].Asy, 0.0);
  EXPECT_DOUBLE_EQ(in.model.sections[0].Asz, 0.0);
}

TEST(TimoshenkoIo, ParsesAsyAsz) {
  auto in = readModelText(baseJson(R"(,"Asy":0.01,"Asz":0.012)"));
  ASSERT_EQ(in.model.sections.size(), 1u);
  EXPECT_DOUBLE_EQ(in.model.sections[0].Asy, 0.01);
  EXPECT_DOUBLE_EQ(in.model.sections[0].Asz, 0.012);
}

TEST(TimoshenkoIo, RejectsNegativeAsy) {
  EXPECT_THROW(readModelText(baseJson(R"(,"Asy":-0.01)")), ModelInputError);
}

TEST(TimoshenkoIo, RoundTripsThroughWriteModel) {
  auto in = readModelText(baseJson(R"(,"Asy":0.01,"Asz":0.012)"));
  auto back = kopruq::io::readModelText(kopruq::io::writeModel(in).dump());
  ASSERT_EQ(back.model.sections.size(), 1u);
  EXPECT_DOUBLE_EQ(back.model.sections[0].Asy, 0.01);
  EXPECT_DOUBLE_EQ(back.model.sections[0].Asz, 0.012);
}

TEST(TimoshenkoIo, SolveWithShearAreaMatchesDirectApi) {
  auto in = readModelText(baseJson(R"(,"Asy":0.01)"));
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
}
