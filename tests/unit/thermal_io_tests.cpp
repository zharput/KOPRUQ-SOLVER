// Phase 7T — JSON model input/output for material thermal_expansion and
// load_case.thermal (MemberThermalLoad). Numerical correctness is
// independently verified in thermal_load_tests.cpp; this file checks the
// reader/writer wiring and input rejections.
#include <gtest/gtest.h>
#include "kopruq/io/ModelJson.hpp"

using kopruq::io::ModelInputError;
using kopruq::io::readModelText;

namespace {
std::string baseJson(const char* thermalArray) {
  return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3,"thermal_expansion":1.2e-5}],
    "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":1e-4,"J":1e-4}],
    "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}],
    "load_cases":[{"name":"T","thermal":)") + thermalArray + R"(}]
  })";
}
}

TEST(ThermalIo, ParsesUniformAndGradientFields) {
  auto in = readModelText(baseJson(R"([{"element":7,"uniform_delta_t":30,"gradient_z_delta_t":10,"gradient_z_depth":0.8}])"));
  ASSERT_EQ(in.load_cases.size(), 1u);
  ASSERT_EQ(in.load_cases[0].thermal.size(), 1u);
  const auto& t = in.load_cases[0].thermal[0];
  EXPECT_EQ(t.element, 7u);
  EXPECT_DOUBLE_EQ(t.uniform_delta_t, 30.0);
  EXPECT_DOUBLE_EQ(t.gradient_z_delta_t, 10.0);
  EXPECT_DOUBLE_EQ(t.gradient_z_depth, 0.8);
  EXPECT_DOUBLE_EQ(in.model.materials[0].thermal_expansion, 1.2e-5);
}

TEST(ThermalIo, DefaultsAreZero) {
  auto in = readModelText(baseJson(R"([{"element":7}])"));
  const auto& t = in.load_cases[0].thermal[0];
  EXPECT_DOUBLE_EQ(t.uniform_delta_t, 0.0);
  EXPECT_DOUBLE_EQ(t.gradient_y_delta_t, 0.0);
  EXPECT_DOUBLE_EQ(t.gradient_z_delta_t, 0.0);
}

TEST(ThermalIo, RoundTripsThroughWriteModel) {
  auto in = readModelText(baseJson(R"([{"element":7,"uniform_delta_t":30,"gradient_z_delta_t":10,"gradient_z_depth":0.8}])"));
  auto back = kopruq::io::readModelText(kopruq::io::writeModel(in).dump());
  ASSERT_EQ(back.load_cases[0].thermal.size(), 1u);
  EXPECT_DOUBLE_EQ(back.load_cases[0].thermal[0].uniform_delta_t, 30.0);
  EXPECT_DOUBLE_EQ(back.model.materials[0].thermal_expansion, 1.2e-5);
}

TEST(ThermalIo, EndToEndSolveMatchesDirectApi) {
  auto in = readModelText(baseJson(R"([{"element":7,"uniform_delta_t":30}])"));
  auto r = kopruq::LinearStaticSolver::solveCases(in.model, in.restraints, in.load_cases, in.combinations);
  ASSERT_TRUE(r.status.success) << r.status.message;
  double ux2 = 0; for (const auto& n : r.cases.front().results.nodes) if (n.node_id == 2) ux2 = n.displacement[0];
  EXPECT_NEAR(ux2, 1.2e-5*30.0*4.0, 1e-9);
}

TEST(ThermalIo, RejectsUnknownElement) {
  EXPECT_THROW(readModelText(baseJson(R"([{"element":99,"uniform_delta_t":10}])")), ModelInputError);
}

TEST(ThermalIo, RejectsGradientWithoutDepth) {
  EXPECT_THROW(readModelText(baseJson(R"([{"element":7,"gradient_y_delta_t":10}])")), ModelInputError);
}

TEST(ThermalIo, NegativeMaterialThermalExpansionIsAccepted) {
  // Negative thermal expansion is physically unusual but not rejected -- not every material's
  // coefficient is conventionally positive, and Material::validate() only requires it finite.
  std::string text = baseJson("[]");
  const std::string from = "\"thermal_expansion\":1.2e-5", to = "\"thermal_expansion\":-1.2e-5";
  text.replace(text.find(from), from.size(), to);
  auto in = readModelText(text);
  EXPECT_DOUBLE_EQ(in.model.materials[0].thermal_expansion, -1.2e-5);
  EXPECT_NO_THROW(in.model.materials[0].validate());
}
