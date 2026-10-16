// Phase 3.2R5 follow-up — solver diagnostics requalification.
//
// Covers result fields that were declared but never populated before this
// change: StaticSolution::residual_norm / relative_residual, the
// NonFiniteSolution status, and AnalysisResults' aggregate
// element_equilibrium_error / element_moment_equilibrium_error. Expected
// values come from an independently written dense Kff and from test-local
// aggregation of the per-element end forces (themselves verified in R4).
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Real;
using Vec6 = Eigen::Matrix<Real,6,1>;

namespace {

kopruq::Model cantilever(double E=210e6, double L=4.0) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L,0,0}};
  m.materials={{1,E,.3}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{81,1,2,1,1,{0,1,0}}};
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixedNode1() {
  return {{1,{true,true,true,true,true,true}}};
}

// Independent 12x12 Euler-Bernoulli local stiffness (not kopruq::Beam3D).
Eigen::MatrixXd indepLocalK(double L,double E,double G,double A,double Iy,double Iz,double J) {
  Eigen::MatrixXd k=Eigen::MatrixXd::Zero(12,12);
  double ea=E*A/L, gj=G*J/L;
  double ay=12*E*Iz/(L*L*L), by=6*E*Iz/(L*L), cy=4*E*Iz/L, dy=2*E*Iz/L;
  double az=12*E*Iy/(L*L*L), bz=6*E*Iy/(L*L), cz=4*E*Iy/L, dz=2*E*Iy/L;
  k(0,0)=k(6,6)=ea; k(0,6)=k(6,0)=-ea;
  k(3,3)=k(9,9)=gj; k(3,9)=k(9,3)=-gj;
  k(1,1)=k(7,7)=ay; k(1,7)=k(7,1)=-ay; k(1,5)=k(5,1)=by; k(1,11)=k(11,1)=by;
  k(7,5)=k(5,7)=-by; k(7,11)=k(11,7)=-by; k(5,5)=k(11,11)=cy; k(5,11)=k(11,5)=dy;
  k(2,2)=k(8,8)=az; k(2,8)=k(8,2)=-az; k(2,4)=k(4,2)=-bz; k(2,10)=k(10,2)=-bz;
  k(8,4)=k(4,8)=bz; k(8,10)=k(10,8)=bz; k(4,4)=k(10,10)=cz; k(4,10)=k(10,4)=dz;
  return k;
}

} // namespace

TEST(SolverDiagnostics, ResidualFieldsMatchIndependentDenseDefinition) {
  double E=210e6, L=4.0;
  Vec6 load; load<<80,-40,25,12,-7,15;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(E,L),fixedNode1(),{{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  ASSERT_EQ(s.free.size(),6);

  // Beam along global x with reference y = global y: local axes == global axes,
  // so with node 1 fixed, Kff is the node-2 block of the local stiffness.
  Eigen::MatrixXd Kff=indepLocalK(L,E,E/(2*1.3),.3,.02,.04,.01).block(6,6,6,6);
  Eigen::VectorXd u=s.free, F=load;
  double indepResidual=(Kff*u-F).norm();
  double denom=Kff.norm()*u.norm()+F.norm();

  EXPECT_TRUE(std::isfinite(s.residual_norm));
  EXPECT_GE(s.residual_norm,0.0);
  EXPECT_LT(s.relative_residual,1e-14);
  EXPECT_LT(indepResidual/denom,1e-14); // the solution satisfies the independent equations too
  // relative_residual uses the documented normalization ||r|| / (||Kff||_F ||u|| + ||F||).
  EXPECT_NEAR(s.relative_residual, s.residual_norm/denom, 1e-12*std::max(s.relative_residual,1e-300));
}

TEST(SolverDiagnostics, ZeroLoadGivesZeroResidualAndErrorsNotNaN) {
  auto s=kopruq::LinearStaticSolver::solve(cantilever(),fixedNode1(),{});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  EXPECT_EQ(s.free.cwiseAbs().maxCoeff(),0.0);
  EXPECT_EQ(s.residual_norm,0.0);
  EXPECT_EQ(s.relative_residual,0.0);
  EXPECT_EQ(s.results.element_equilibrium_error,0.0);
  EXPECT_EQ(s.results.element_moment_equilibrium_error,0.0);
}

TEST(SolverDiagnostics, NonFiniteSolutionIsReported) {
  // Every stiffness term stays a normal (non-zero, non-denormal) double, so
  // the factorization succeeds, but F*L/(E*A) = 1e308*4/(1e-300*0.3)
  // overflows: the axial free displacement is +inf.
  Vec6 load; load<<1e308,0,0,0,0,0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(1e-300),fixedNode1(),{{2,load}});
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code,kopruq::SolverStatusCode::NonFiniteSolution)<<s.status.message;
}

TEST(SolverDiagnostics, AggregateElementEquilibriumIsForceWeightedAndBoundedByMax) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,0,0,3},{3,4,0,3}};
  m.materials={{1,210e6,.3}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{91,1,2,1,1,{1,0,0}},{92,2,3,1,1,{0,0,1}}};
  Vec6 load; load<<25,-35,10,4,-6,8;
  auto s=kopruq::LinearStaticSolver::solve(m,fixedNode1(),{{3,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  ASSERT_EQ(s.results.elements.size(),2u);

  // Recompute the documented aggregates from the per-element end forces.
  std::map<kopruq::Id,Eigen::Vector3d> xyz={{1,{0,0,0}},{2,{0,0,3}},{3,{4,0,3}}};
  std::map<kopruq::Id,std::pair<kopruq::Id,kopruq::Id>> ends={{91,{1,2}},{92,{2,3}}};
  double sfb=0,sfs=0,smb=0,sms=0;
  for(const auto& e:s.results.elements){
    Eigen::Vector3d f1,m1,f2,m2;
    for(int i=0;i<3;++i){f1(i)=e.global_end_force[i];m1(i)=e.global_end_force[3+i];f2(i)=e.global_end_force[6+i];m2(i)=e.global_end_force[9+i];}
    Eigen::Vector3d r=xyz[ends[e.element_id].second]-xyz[ends[e.element_id].first];
    sfb+=(f1+f2).norm(); sfs+=f1.norm()+f2.norm();
    smb+=(m1+m2+r.cross(f2)).norm(); sms+=m1.norm()+m2.norm()+r.norm()*(f1.norm()+f2.norm());
  }
  ASSERT_GT(sfs,0.0); ASSERT_GT(sms,0.0);
  double expectedF=sfb/sfs, expectedM=smb/sms;
  const auto& r=s.results;
  EXPECT_NEAR(r.element_equilibrium_error,expectedF,1e-6*expectedF+1e-30);
  EXPECT_NEAR(r.element_moment_equilibrium_error,expectedM,1e-6*expectedM+1e-30);
  // A force-weighted mean of per-element ratios can never exceed their maximum.
  EXPECT_LE(r.element_equilibrium_error,r.max_element_force_error*(1+1e-12)+1e-300);
  EXPECT_LE(r.element_moment_equilibrium_error,r.max_element_moment_error*(1+1e-12)+1e-300);
  EXPECT_LT(r.element_equilibrium_error,1e-12);
  EXPECT_LT(r.element_moment_equilibrium_error,1e-12);
}
