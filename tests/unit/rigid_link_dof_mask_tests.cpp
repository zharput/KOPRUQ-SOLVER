// 2026-11-21 -- per-DOF rigid links (RigidLink::dofs, MIDAS "DOF of Rigid Link") and the MIDAS
// importer rule that a restrained slave DOF is dropped from the link (evidence: verification
// model 23, see MidasJsonImporter.hpp). Dedicated new file.
//
// Closed-form reference (independent of RigidLinkReduction): cantilever 1-2 along X (length L,
// Euler-Bernoulli, bending in X-Z about local y = global Y), slave node 3 at (L, 0, h) tied to
// master 2 in UX and RY only; node 3's untied UY, UZ, RX, RZ are restrained. A force P along X at
// node 3 reaches the master, by virtual work through the tied DOFs only (du3x = du2x + h dth2y),
// as Fx = P and My = h P. Hence u2x = P L / (E A), and the cantilever under tip moment My gives
// w2 = -My L^2 / (2 E Iy) (sign from the stiffness relation below), th2y = My L / (E Iy). The slave
// follows u3x = u2x + h th2y, th3y = th2y, but its UZ stays 0 although the master's w2 != 0 (the
// model-23 situation). Base reactions: FX = -P, MY = -h P; node 3 carries no reaction.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include <string>
#include "kopruq/assembly/RigidLink.hpp"
#include "kopruq/solvers/LinearStaticSolver.hpp"
#include "kopruq/io/ModelJson.hpp"
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
constexpr double E = 2e8, A = 0.02, IY = 1e-4, IZ = 2e-4, J = 3e-4, L = 4.0, H = 0.5, P = 1000.0;
Model partialLinkModel() {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}, {3, L, 0, H}};
  m.materials = {{1, E, 0.3, 0}};
  m.sections = {{1, A, IY, IZ, J}};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  kopruq::RigidLink l; l.id = 1; l.master = 2; l.slave = 3; l.dofs = {true, false, false, false, true, false};
  m.rigid_links = {l};
  return m;
}
std::map<Id, std::array<bool,6>> partialLinkRestraints() {
  return {{1, {true, true, true, true, true, true}}, {3, {false, true, true, true, false, true}}};
}
}

TEST(RigidLinkDofMask, PartialLinkMatchesClosedForm) {
  auto m = partialLinkModel();
  LoadCase lc; lc.nodal = {NodalLoad{3, (Eigen::Matrix<double,6,1>() << P, 0, 0, 0, 0, 0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, partialLinkRestraints(), {lc}).cases.front();
  ASSERT_TRUE(sol.status.success) << sol.status.message;

  const double My = H * P, EI = E * IY;
  // Cantilever tip stiffness (Euler-Bernoulli, local z = global Z, theta_y = -dw/dx):
  // [Fz; My] = [12EI/L^3, -6EI/L^2; -6EI/L^2, 4EI/L] [w; th]; with Fz = 0: th = My L/(EI), w = -My L^2/(2EI).
  const double th = My * L / EI, w = -My * L * L / (2 * EI), u2 = P * L / (E * A);
  const auto& master = nodeOf(sol, 2);
  rel(master.displacement[0], u2, "master UX = P L / EA");
  rel(master.displacement[4], th, "master RY = h P L / EI");
  rel(master.displacement[2], w, "master UZ = -h P L^2 / 2EI");
  const auto& slave = nodeOf(sol, 3);
  rel(slave.displacement[0], u2 + H * th, "slave UX = master UX + h RY (tied)");
  rel(slave.displacement[4], th, "slave RY = master RY (tied)");
  EXPECT_EQ(slave.displacement[2], 0.0) << "slave UZ is restrained, not tied, so it stays 0 while the master moves";
  for (int i = 0; i < 6; ++i) EXPECT_NEAR(slave.reaction[i], 0.0, 1e-9) << "slave reaction " << i;
  const auto& base = nodeOf(sol, 1);
  rel(base.reaction[0], -P, "base FX");
  rel(base.reaction[4], -H * P, "base MY");
  EXPECT_NEAR(base.reaction[2], 0.0, 1e-7);
  EXPECT_LT(sol.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(sol.results.energy_error, 1e-8);
}

TEST(RigidLinkDofMask, ReductionKeepsUntiedSlaveDofs) {
  auto m = partialLinkModel();
  kopruq::DofManager dm(m);
  auto red = kopruq::RigidLinkReduction::build(m, dm);
  EXPECT_EQ(red.reduced_count(), 18u - 2u); // only the 2 tied slave DOFs are eliminated
  EXPECT_TRUE(red.isTied(3, 0));
  EXPECT_FALSE(red.isTied(3, 2));
  EXPECT_TRUE(red.isTied(3, 4));
}

TEST(RigidLinkDofMask, RestrainingATiedDofIsStillRejected) {
  auto m = partialLinkModel();
  auto rs = partialLinkRestraints();
  rs[3][0] = true; // UX is tied
  LoadCase lc; lc.nodal = {NodalLoad{3, (Eigen::Matrix<double,6,1>() << P, 0, 0, 0, 0, 0).finished()}};
  auto sol = kopruq::LinearStaticSolver::solveCases(m, rs, {lc});
  EXPECT_FALSE(sol.status.success);
  EXPECT_NE(sol.status.message.find("tied"), std::string::npos) << sol.status.message;
}

TEST(RigidLinkDofMask, ModelJsonReadsAndWritesDofs) {
  const std::string text = R"({
    "schema":"kopruq-model","schema_version":1,
    "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0},{"id":3,"x":4,"y":0,"z":0.5}],
    "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
    "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
    "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
    "rigid_links":[{"id":1,"master":2,"slave":3,"dofs":[true,false,false,false,true,false]}],
    "supports":[{"node":1,"restraints":[true,true,true,true,true,true]},{"node":3,"restraints":[false,true,true,true,false,true]}],
    "load_cases":[{"name":"c","nodal":[{"node":3,"values":[1000,0,0,0,0,0]}]}]
  })";
  auto in = kopruq::io::readModelText(text);
  ASSERT_EQ(in.model.rigid_links.size(), 1u);
  const std::array<bool,6> expected{true, false, false, false, true, false};
  EXPECT_EQ(in.model.rigid_links[0].dofs, expected);
  auto j = kopruq::io::writeModel(in);
  EXPECT_EQ(j["rigid_links"][0]["dofs"].get<std::vector<bool>>(), std::vector<bool>(expected.begin(), expected.end()));
  // A restraint on a tied DOF is rejected by the reader.
  std::string bad = text;
  const std::string untiedOnly = "[false,true,true,true,false,true]";
  bad.replace(bad.find(untiedOnly), untiedOnly.size(), "[true,true,true,true,false,true]");
  EXPECT_THROW(kopruq::io::readModelText(bad), kopruq::io::ModelInputError);
}

namespace {
nlohmann::json midasModel(const char* dof, const char* slaveConstraint, int styp) {
  return nlohmann::json::parse(std::string(R"json({"DBG":{
    "UNIT":{"1":{"FORCE":"KN","DIST":"M"}},
    "STYP":{"1":{"STYP":)json") + std::to_string(styp) + R"json(}},
    "NODE":{"1":{"X":0.0,"Y":0.0,"Z":0.0},"2":{"X":4.0,"Y":0.0,"Z":0.0},"3":{"X":4.0,"Y":0.0,"Z":-0.5}},
    "MATL":{"1":{"TYPE":"STEEL","NAME":"S355","PARAM":[{"P_TYPE":1,"STANDARD":"EN05(S)","CODE":"","DB":"S355"}]}},
    "SECT":{"1":{"SECTTYPE":"DBUSER","SECT_NAME":"R","SECT_BEFORE":{"SHAPE":"SB","USE_SHEAR_DEFORM":false,"SECT_I":{"vSIZE":[1.0,0.5]}}}},
    "ELEM":{"1":{"TYPE":"BEAM","MATL":1,"SECT":1,"NODE":[1,2,0,0,0,0,0,0],"ANGLE":0.0}},
    "CONS":{"1":{"ITEMS":[{"ID":1,"CONSTRAINT":"1111111"}]},"3":{"ITEMS":[{"ID":1,"CONSTRAINT":")json" + std::string(slaveConstraint) + R"json("}]}},
    "RIGD":{"2":{"ITEMS":[{"ID":1,"GROUP_NAME":"","DOF":)json" + dof + R"json(,"S_NODE":[3]}]}}}})json");
}
}

TEST(RigidLinkDofMask, MidasRestrainedSlaveDofsAreDroppedFromTheLink) {
  // Model-23 pattern: DOF 111111, slave CONS 0111000 (DY DZ RX) and STYP=1 (UY RX RZ everywhere):
  // tied DOFs left = DX, RY.
  const auto r = kopruq::io::midas::importMidasJson(midasModel("111111", "0111000", 1));
  ASSERT_EQ(r.input.model.rigid_links.size(), 1u);
  const std::array<bool,6> expected{true, false, false, false, true, false};
  EXPECT_EQ(r.input.model.rigid_links[0].dofs, expected);
  bool reported = false;
  for (const auto& a : r.report.assumptions) if (a.find("restrained slave DOF(s) DY DZ RX RZ removed") != std::string::npos) reported = true;
  EXPECT_TRUE(reported);
  auto sol = kopruq::LinearStaticSolver::solveCases(r.input.model, r.input.restraints, r.input.load_cases);
  EXPECT_TRUE(sol.status.success) << sol.status.message;
}

TEST(RigidLinkDofMask, MidasIntegerDofLosesLeadingZeros) {
  // DOF exported as the integer 11 means "000011" = RY, RZ; 3D model, unrestrained slave.
  const auto r = kopruq::io::midas::importMidasJson(midasModel("11", "0000000", 0));
  ASSERT_EQ(r.input.model.rigid_links.size(), 1u);
  const std::array<bool,6> expected{false, false, false, false, true, true};
  EXPECT_EQ(r.input.model.rigid_links[0].dofs, expected);
  const auto bad = kopruq::io::midas::importMidasJson(midasModel("1211", "0000000", 0));
  EXPECT_TRUE(bad.input.model.rigid_links.empty());
}
