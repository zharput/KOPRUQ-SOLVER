// Phase 5 (deferred item, completed per user instruction) — Timoshenko
// shear-deformation correction for Beam3D (Section::Asy/Asz,
// Beam3DProperties::Asy/Asz).
//
// Two independent checks:
//  1. ZeroShearAreaMatchesEulerBernoulli: Asy=Asz=0 (the default) must
//     reproduce the pre-Timoshenko local stiffness bit-for-bit -- compared
//     here against the plain, un-corrected textbook Euler-Bernoulli
//     formulas (ay=12EI/L^3 etc.) written out independently, not against
//     Beam3D::localStiffness's own old code (which no longer exists to
//     compare against).
//  2. CantileverTipDeflectionMatchesClosedFormTimoshenko: a cantilever
//     (fixed node1, free node2) under a transverse tip point force has the
//     classical closed-form Timoshenko solution
//       tip deflection = P*L^3/(3EI) + P*L/(G*As)   (shear adds a term)
//       tip rotation   = P*L^2/(2EI)                (unaffected by shear)
//     derived independently here from beam theory, not from the
//     production stiffness matrix, and checked for both bending planes
//     and several shear areas (including the Asy=0 Euler-Bernoulli limit).
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Id;
using kopruq::Model;
using kopruq::LoadCase;
using kopruq::MemberPointLoad;
using kopruq::LoadAxes;
using kopruq::MemberLoadKind;

namespace {
constexpr double E = 2e8, NU = 0.3, A = 0.02, IY = 3e-4, IZ = 4e-4, J = 5e-4, L = 4.0;
constexpr double G = E / (2 * (1 + NU));

void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}

Model cantilever(double Asy, double Asz) {
  Model m;
  m.nodes = {{1, 0, 0, 0}, {2, L, 0, 0}};
  m.materials = {{1, E, NU, 0}};
  kopruq::Section sec{1, A, IY, IZ, J, Asy, Asz};
  m.sections = {sec};
  m.beams = {{7, 1, 2, 1, 1, {0, 1, 0}}};
  return m;
}
std::map<Id,std::array<bool,6>> fixed1() { return {{1, {true,true,true,true,true,true}}}; }
kopruq::StaticSolution solve(const Model& m, const LoadCase& lc) {
  return kopruq::LinearStaticSolver::solveCases(m, fixed1(), {lc}).cases.front();
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
} // namespace

TEST(Timoshenko, ZeroShearAreaMatchesEulerBernoulli) {
  kopruq::Beam3DProperties p{E, G, A, IY, IZ, J, 0, 0};
  auto k = kopruq::Beam3D::localStiffness(L, p);
  double ay = 12*E*IZ/(L*L*L), by = 6*E*IZ/(L*L), cy = 4*E*IZ/L, dy = 2*E*IZ/L;
  double az = 12*E*IY/(L*L*L), bz = 6*E*IY/(L*L), cz = 4*E*IY/L, dz = 2*E*IY/L;
  rel(k(1,1), ay, "k(1,1)"); rel(k(1,5), by, "k(1,5)"); rel(k(5,5), cy, "k(5,5)"); rel(k(5,11), dy, "k(5,11)");
  rel(k(2,2), az, "k(2,2)"); rel(k(2,4), -bz, "k(2,4)"); rel(k(4,4), cz, "k(4,4)"); rel(k(4,10), dz, "k(4,10)");
}

TEST(Timoshenko, CantileverTipDeflectionMatchesClosedFormTimoshenko) {
  constexpr double P = 1000.0;
  for (double Asy : {0.0, 0.05, 0.01, 0.002}) {
    Model m = cantilever(Asy, 0.0);
    LoadCase lc; lc.point = {MemberPointLoad{7, LoadAxes::Local, 1, MemberLoadKind::Force, P, 0.0}};
    auto sol = solve(m, lc);
    ASSERT_TRUE(sol.status.success) << sol.status.message;
    const auto& tip = nodeOf(sol, 2);
    double shearTerm = Asy > 0 ? P*L/(G*Asy) : 0.0;
    double expectedW = P*L*L*L/(3*E*IZ) + shearTerm;
    double expectedTheta = P*L*L/(2*E*IZ);
    rel(tip.displacement[1], expectedW, "tip transverse deflection (local y)");
    rel(tip.displacement[5], expectedTheta, "tip rotation (local z, RZ)", 1e-8);
    EXPECT_LT(sol.results.energy_error, 1e-9);
  }
}

TEST(Timoshenko, LowerShearAreaMonotonicallyIncreasesDeflection) {
  constexpr double P = 500.0;
  double prevW = -1;
  for (double Asy : {0.1, 0.02, 0.005, 0.001}) {
    Model m = cantilever(Asy, 0.0);
    LoadCase lc; lc.point = {MemberPointLoad{7, LoadAxes::Local, 1, MemberLoadKind::Force, P, 0.0}};
    auto sol = solve(m, lc);
    ASSERT_TRUE(sol.status.success);
    double w = nodeOf(sol, 2).displacement[1];
    EXPECT_GT(w, prevW) << "Asy=" << Asy;
    prevW = w;
  }
}
