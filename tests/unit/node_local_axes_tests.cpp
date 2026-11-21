// 2026-11-21 -- node local axes (Model::node_axes, MIDAS "Node Local Axis" / SKEW): restraint flags
// act along the node's local axes and the reaction is also reported in them. Dedicated new file.
// Expected values are closed-form statics / stiffness relations derived here, not produced by the
// transformation layer under test.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include "kopruq/solvers/LinearStaticSolver.hpp"
#include "kopruq/solvers/ModalSolver.hpp"
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"
#include "kopruq/io/ModelJson.hpp"
#include "kopruq/io/ResultsJson.hpp"
#include "kopruq/io/midas/MidasJsonImporter.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::NodalLoad;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
Eigen::Matrix3d aboutZ(double deg) {
  const double a = deg * std::acos(-1.0) / 180, c = std::cos(a), s = std::sin(a);
  Eigen::Matrix3d R; R << c, s, 0, -s, c, 0, 0, 0, 1; // rows: local x, y, z
  return R;
}
constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4, L = 4.0, P = 1000.0;
Model cantilever() {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, 0.3, 78.5}};
  m.sections = {{1, A, IY, IZ, J}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  return m;
}
const std::array<bool,6> kAll{true, true, true, true, true, true};
}

TEST(NodeLocalAxes, FullyFixedSkewSupportReportsLocalReaction) {
  auto m = cantilever();
  m.node_axes[1] = aboutZ(30);
  LoadCase lc; lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << 0, 0, -P, 0, 0, 0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, {{1, kAll}}, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const auto& base = nodeOf(sol, 1);
  // Global statics: F = (0, 0, P); M = -(r x F_applied) = -((L,0,0) x (0,0,-P)) = (0, -L P, 0).
  rel(base.reaction[2], P, "global FZ"); rel(base.reaction[4], -L * P, "global MY");
  EXPECT_NEAR(base.reaction[0], 0, 1e-8); EXPECT_NEAR(base.reaction[3], 0, 1e-8); EXPECT_NEAR(base.reaction[5], 0, 1e-8);
  // Local (x = (c, s, 0), y = (-s, c, 0)): M_local = (-L P s, -L P c, 0), F_local = (0, 0, P).
  const double c = std::cos(std::acos(-1.0) / 6), s = 0.5;
  EXPECT_TRUE(base.local_axes);
  rel(base.reaction_local[2], P, "local Fz");
  rel(base.reaction_local[3], -L * P * s, "local Mx");
  rel(base.reaction_local[4], -L * P * c, "local My");
  EXPECT_NEAR(base.reaction_local[5], 0, 1e-8);
  // The tip deflection is the plain cantilever one: a fully fixed node's axes do not matter.
  rel(nodeOf(sol, 2).displacement[2], -P * L * L * L / (3 * E * IY), "tip UZ");
  EXPECT_FALSE(nodeOf(sol, 2).local_axes);
  EXPECT_EQ(nodeOf(sol, 2).reaction_local, nodeOf(sol, 2).reaction);
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
}

TEST(NodeLocalAxes, InclinedRollerMatchesClosedForm) {
  // Tip node 2 restrained only along its local x = n = (cos 45, sin 45, 0); load P along global Y.
  // Tip stiffnesses (Euler-Bernoulli, rotation free): ka = EA/L along X, ky = 3 E Iz / L^3 along Y.
  // Equilibrium ka ux = Rn c, ky uy = P + Rn s, constraint c ux + s uy = 0
  //   => Rn = -s P / (ky c^2 / ka + s^2).
  auto m = cantilever();
  m.node_axes[2] = aboutZ(45);
  LoadCase lc; lc.nodal = {NodalLoad{2, (Eigen::Matrix<double,6,1>() << 0, P, 0, 0, 0, 0).finished()}};
  std::map<Id, std::array<bool,6>> rs{{1, kAll}, {2, {true, false, false, false, false, false}}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;
  const double c = std::sqrt(0.5), s = c, ka = E * A / L, ky = 3 * E * IZ / (L * L * L);
  const double Rn = -s * P / (ky * c * c / ka + s * s);
  const auto& tip = nodeOf(sol, 2);
  rel(tip.reaction_local[0], Rn, "local reaction along the roller normal");
  for (int i = 1; i < 6; ++i) EXPECT_NEAR(tip.reaction_local[i], 0, 1e-8) << "unrestrained local direction " << i;
  rel(tip.reaction[0], Rn * c, "global FX"); rel(tip.reaction[1], Rn * s, "global FY");
  rel(tip.displacement[0], Rn * c / ka, "UX");
  rel(tip.displacement[1], (P + Rn * s) / ky, "UY");
  EXPECT_NEAR(c * tip.displacement[0] + s * tip.displacement[1], 0, 1e-12) << "no motion along the roller normal";
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);

  const auto j = kopruq::io::solutionJson("c", sol);
  bool sawTip = false, sawBase = false;
  for (const auto& n : j["nodes"]) {
    if (n["id"] == 2) { sawTip = true; ASSERT_TRUE(n.contains("reaction_local")); EXPECT_NEAR(n["reaction_local"][0].get<double>(), Rn, 1e-9 * std::abs(Rn)); }
    if (n["id"] == 1) { sawBase = true; EXPECT_FALSE(n.contains("reaction_local")); }
  }
  EXPECT_TRUE(sawTip && sawBase);
}

TEST(NodeLocalAxes, ModalUnchangedByFullyFixedSkewSupport) {
  auto plain = cantilever();
  auto skew = plain; skew.node_axes[1] = aboutZ(30);
  auto a = kopruq::ModalSolver::solve(plain, {{1, kAll}}, 4);
  auto b = kopruq::ModalSolver::solve(skew, {{1, kAll}}, 4);
  ASSERT_TRUE(a.status.success && b.status.success) << b.status.message;
  for (std::size_t i = 0; i < a.modes.size(); ++i) rel(b.modes[i].frequency_hz, a.modes[i].frequency_hz, "frequency", 1e-9);
}

TEST(NodeLocalAxes, ResponseSpectrumRejectsNodeAxes) {
  auto m = cantilever(); m.node_axes[1] = aboutZ(30);
  kopruq::ResponseSpectrumInput in; in.direction = 0; in.num_modes = 2;
  in.spectrum = {{0.01, 1.0}, {10.0, 1.0}};
  auto r = kopruq::ResponseSpectrumSolver::solve(m, {{1, kAll}}, in);
  EXPECT_FALSE(r.status.success);
  EXPECT_NE(r.status.message.find("node local axes"), std::string::npos) << r.status.message;
}

TEST(NodeLocalAxes, ModelJsonReadsWritesAndValidates) {
  auto text = [](const char* axes) {
    return std::string(R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
    "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
    "node_axes":)") + axes + R"(,
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}],
    "load_cases":[{"name":"c","nodal":[{"node":2,"values":[0,0,-1000,0,0,0]}]}]})";
  };
  const double h = std::sqrt(0.5);
  auto in = kopruq::io::readModelText(text(R"([{"node":1,"local_x":[0.7071067811865476,0.7071067811865476,0],"local_y":[-0.7071067811865476,0.7071067811865476,0]}])"));
  ASSERT_EQ(in.model.node_axes.size(), 1u);
  const auto& R = in.model.node_axes.at(1);
  rel(R(0, 0), h, "x.x"); rel(R(1, 0), -h, "y.x"); rel(R(2, 2), 1.0, "z.z");
  auto j = kopruq::io::writeModel(in);
  ASSERT_TRUE(j.contains("node_axes"));
  rel(j["node_axes"][0]["local_y"][1].get<double>(), h, "written local_y.y");
  EXPECT_THROW(kopruq::io::readModelText(text(R"([{"node":1,"local_x":[1,0,0],"local_y":[1,1,0]}])")), kopruq::io::ModelInputError);
  EXPECT_THROW(kopruq::io::readModelText(text(R"([{"node":9,"local_x":[1,0,0],"local_y":[0,1,0]}])")), kopruq::io::ModelInputError);
  // Without node axes the writer output carries no node_axes key (existing models unchanged).
  auto plain = in; plain.model.node_axes.clear();
  EXPECT_FALSE(kopruq::io::writeModel(plain).contains("node_axes"));
}

namespace {
nlohmann::json midasSkew(const char* skew, const char* baseConstraint, int styp) {
  return nlohmann::json::parse(std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "STYP":{"1":{"STYP":)json") + std::to_string(styp) + R"json(}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":")json" + std::string(baseConstraint) + R"json("}]}},
    "SKEW":{"1":)json" + skew + R"json(}}})json");
}
bool anyContains(const std::vector<std::string>& v, const char* s) {
  for (const auto& x : v) if (x.find(s) != std::string::npos) return true;
  return false;
}
}

TEST(NodeLocalAxes, MidasSkewAngleZ) {
  const auto r = kopruq::io::midas::importMidasJson(midasSkew(R"({"iMETHOD":1,"ANGLE_X":0.0,"ANGLE_Y":0.0,"ANGLE_Z":45.0})", "1111111", 0));
  ASSERT_EQ(r.input.model.node_axes.size(), 1u);
  const auto& R = r.input.model.node_axes.at(1);
  const double h = std::sqrt(0.5);
  rel(R(0, 0), h, "x.x"); rel(R(0, 1), h, "x.y"); rel(R(1, 0), -h, "y.x"); rel(R(1, 1), h, "y.y"); rel(R(2, 2), 1.0, "z.z");
}

TEST(NodeLocalAxes, MidasSkewUnverifiedCasesAreRejected) {
  auto two = kopruq::io::midas::importMidasJson(midasSkew(R"({"iMETHOD":1,"ANGLE_X":10.0,"ANGLE_Y":0.0,"ANGLE_Z":45.0})", "1111111", 0));
  EXPECT_TRUE(two.input.model.node_axes.empty());
  EXPECT_TRUE(anyContains(two.report.rejected, "more than one nonzero rotation angle"));
  auto method = kopruq::io::midas::importMidasJson(midasSkew(R"({"iMETHOD":2,"ANGLE_X":0.0,"ANGLE_Y":0.0,"ANGLE_Z":45.0})", "1111111", 0));
  EXPECT_TRUE(method.input.model.node_axes.empty());
  EXPECT_TRUE(anyContains(method.report.rejected, "iMETHOD 2"));
  auto planar = kopruq::io::midas::importMidasJson(midasSkew(R"({"iMETHOD":1,"ANGLE_X":0.0,"ANGLE_Y":0.0,"ANGLE_Z":45.0})", "1110000", 1));
  EXPECT_TRUE(planar.input.model.node_axes.empty());
  EXPECT_TRUE(anyContains(planar.report.rejected, "STYP=1"));
  auto planarFixed = kopruq::io::midas::importMidasJson(midasSkew(R"({"iMETHOD":1,"ANGLE_X":0.0,"ANGLE_Y":0.0,"ANGLE_Z":45.0})", "1111111", 1));
  EXPECT_EQ(planarFixed.input.model.node_axes.size(), 1u);
}

TEST(NodeLocalAxes, SchemaItemMatchesReaderKeys) {
  // schemas/model.schema.json keeps the node_axes item schema inline (properties.node_axes.items);
  // it must list exactly the reader's keys (model_keys::node_axes), all required.
  std::ifstream in(std::string(KOPRUQ_SOURCE_DIR) + "/schemas/model.schema.json");
  ASSERT_TRUE(in.good());
  const auto schema = nlohmann::json::parse(in);
  const auto& item = schema.at("properties").at("node_axes").at("items");
  std::vector<std::string> props, req;
  for (auto it = item.at("properties").begin(); it != item.at("properties").end(); ++it) props.push_back(it.key());
  for (const auto& r : item.at("required")) req.push_back(r.get<std::string>());
  std::vector<std::string> keys = kopruq::io::model_keys::node_axes.required;
  std::sort(props.begin(), props.end()); std::sort(req.begin(), req.end()); std::sort(keys.begin(), keys.end());
  EXPECT_TRUE(kopruq::io::model_keys::node_axes.optional.empty());
  EXPECT_EQ(props, keys);
  EXPECT_EQ(req, keys);
  EXPECT_EQ(item.at("additionalProperties"), false);
  const auto& link = schema.at("$defs").at("rigid_link").at("properties");
  EXPECT_TRUE(link.contains("dofs"));
}
