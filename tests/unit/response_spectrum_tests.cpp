// Phase 6.2 — single-direction response spectrum analysis (SRSS).
//
// Independent references: the total-mass identity iota^T*M*iota = sum of
// element masses (a conservation property of any correct consistent/lumped
// mass matrix, re-derived here from rho*A*L directly, not from the
// production mass matrix); mass participation summing to 1.0 when every
// free-DOF mode is included; piecewise-linear spectrum interpolation
// checked against hand values; SRSS re-aggregation from the per-mode
// results already returned; and per-mode participation factors/peak
// response checked against an independent dense eigensolve + independent
// mass matrix (same style as modal_tests.cpp, not reusing ModalSolver's
// or ResponseSpectrumSolver's internals).
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <cmath>
#include "kopruq/solvers/ResponseSpectrumSolver.hpp"

using Mat12 = Eigen::Matrix<double,12,12>;

namespace {

constexpr double E=210e6, NU=.3, A=.3, IY=.02, IZ=.04, J=.01, UW=78.5;
constexpr double G=E/(2*(1+NU)), RHO_A=UW/kopruq::g_standard*A, RHO_J=UW/kopruq::g_standard*J;

kopruq::Model cantilever(double L, int n=1) {
  kopruq::Model m;
  for (int i=0;i<=n;++i) m.nodes.push_back({kopruq::Id(i+1), L*i/n, 0, 0});
  m.materials={{1,E,NU,UW}}; m.sections={{1,A,IY,IZ,J}};
  for (int i=0;i<n;++i) m.beams.push_back({kopruq::Id(i+1),kopruq::Id(i+1),kopruq::Id(i+2),1,1,{0,1,0}});
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixedNode1() { return {{1,{true,true,true,true,true,true}}}; }

// Same independent closed-form mass/stiffness as modal_tests.cpp.
Mat12 independentLocalK(double L) {
  Mat12 k=Mat12::Zero(); double ea=E*A/L, gj=G*J/L;
  double ay=12*E*IZ/(L*L*L), by=6*E*IZ/(L*L), cy=4*E*IZ/L, dy=2*E*IZ/L;
  double az=12*E*IY/(L*L*L), bz=6*E*IY/(L*L), cz=4*E*IY/L, dz=2*E*IY/L;
  k(0,0)=k(6,6)=ea;k(0,6)=k(6,0)=-ea;k(3,3)=k(9,9)=gj;k(3,9)=k(9,3)=-gj;
  k(1,1)=k(7,7)=ay;k(1,7)=k(7,1)=-ay;k(1,5)=k(5,1)=by;k(1,11)=k(11,1)=by;k(7,5)=k(5,7)=-by;k(7,11)=k(11,7)=-by;k(5,5)=k(11,11)=cy;k(5,11)=k(11,5)=dy;
  k(2,2)=k(8,8)=az;k(2,8)=k(8,2)=-az;k(2,4)=k(4,2)=-bz;k(2,10)=k(10,2)=-bz;k(8,4)=k(4,8)=bz;k(8,10)=k(10,8)=bz;k(4,4)=k(10,10)=cz;k(4,10)=k(10,4)=dz;
  return k;
}
Mat12 independentLocalMass(double L) {
  Mat12 m=Mat12::Zero(); double L2=L*L;
  m(0,0)=m(6,6)=RHO_A*L/3; m(0,6)=m(6,0)=RHO_A*L/6;
  m(3,3)=m(9,9)=RHO_J*L/3; m(3,9)=m(9,3)=RHO_J*L/6;
  double c=RHO_A*L/420;
  double My[4][4]={{156,22*L,54,-13*L},{22*L,4*L2,13*L,-3*L2},{54,13*L,156,-22*L},{-13*L,-3*L2,-22*L,4*L2}};
  int yi[4]={1,5,7,11}; for(int a=0;a<4;++a)for(int b=0;b<4;++b)m(yi[a],yi[b])=c*My[a][b];
  double Mz[4][4]={{156,-22*L,54,13*L},{-22*L,4*L2,-13*L,-3*L2},{54,-13*L,156,22*L},{13*L,-3*L2,22*L,4*L2}};
  int zi[4]={2,4,8,10}; for(int a=0;a<4;++a)for(int b=0;b<4;++b)m(zi[a],zi[b])=c*Mz[a][b];
  return m;
}

} // namespace

TEST(ResponseSpectrum, SpectrumInterpolationMatchesHandValues) {
  std::vector<kopruq::ResponseSpectrumPoint> sp={{0.1,1.0},{0.5,2.0},{1.0,1.5}};
  EXPECT_DOUBLE_EQ(kopruq::interpolateSpectrum(sp,0.1), 1.0);
  EXPECT_DOUBLE_EQ(kopruq::interpolateSpectrum(sp,1.0), 1.5);
  EXPECT_NEAR(kopruq::interpolateSpectrum(sp,0.3), 1.0+(0.3-0.1)/(0.5-0.1)*(2.0-1.0), 1e-12);
  EXPECT_NEAR(kopruq::interpolateSpectrum(sp,0.75), 2.0+(0.75-0.5)/(1.0-0.5)*(1.5-2.0), 1e-12);
  EXPECT_THROW(kopruq::interpolateSpectrum(sp,0.05), std::invalid_argument);
  EXPECT_THROW(kopruq::interpolateSpectrum(sp,1.5), std::invalid_argument);
  EXPECT_THROW(kopruq::interpolateSpectrum({{0.1,1.0}},0.1), std::invalid_argument);
  std::vector<kopruq::ResponseSpectrumPoint> bad={{0.5,1.0},{0.1,2.0}};
  EXPECT_THROW(kopruq::interpolateSpectrum(bad,0.3), std::invalid_argument);
}

TEST(ResponseSpectrum, TotalMassMatchesIndependentPhysicalTotal) {
  double L=6.0; int n=3;
  auto m=cantilever(L,n);
  kopruq::ResponseSpectrumInput in; in.direction=1; in.num_modes=1;
  in.spectrum={{0.001,1.0},{1000.0,1.0}}; // flat, irrelevant to this check
  auto r=kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),in);
  // total_mass is computed before any mode loop can fail; even a trivial
  // (num_modes=1) run exposes it.
  double expected=RHO_A*L; // n elements of length L/n, total length L
  EXPECT_NEAR(r.total_mass, expected, 1e-9*expected);
}

TEST(ResponseSpectrum, MassParticipationSumsToOneWithAllModes) {
  auto m=cantilever(5.0,4);
  // free dofs = 4 nodes * 6 = 24 (node1 fixed, 5 nodes total -> 4 free nodes).
  kopruq::ResponseSpectrumInput in; in.direction=2; in.num_modes=24;
  in.spectrum={{0.001,9.81},{1000.0,9.81}};
  auto r=kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),in);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  EXPECT_EQ(r.modes.size(),24u);
  EXPECT_NEAR(r.total_mass_participation_ratio, 1.0, 1e-7);
  for (const auto& mc : r.modes) EXPECT_GE(mc.mass_participation_ratio, -1e-9);
}

TEST(ResponseSpectrum, SingleElementModeMatchesIndependentDenseComputation) {
  double L=5.0;
  auto m=cantilever(L,1);
  kopruq::ResponseSpectrumInput in; in.direction=1; in.num_modes=6;
  in.spectrum={{0.01,4.0},{0.1,8.0},{1.0,8.0},{10.0,1.0}};
  auto r=kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),in);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  ASSERT_EQ(r.modes.size(),6u);

  // Independent: dense K,M (6x6 free system), generalized eigensolve, mass
  // normalization, participation factor using the FULL 12-dof iota/M
  // (ground motion also acts on the restrained node), then compare.
  Mat12 Kg=independentLocalK(L), Mg=independentLocalMass(L);
  Eigen::MatrixXd Kff=Kg.block(6,6,6,6), Mff=Mg.block(6,6,6,6);
  Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> ges(Kff,Mff);
  ASSERT_EQ(ges.info(),Eigen::Success);
  Eigen::VectorXd iota12=Eigen::VectorXd::Zero(12); iota12(1)=1; iota12(7)=1; // UY at both nodes
  Eigen::VectorXd Miota12=Mg*iota12;

  for (int i=0;i<6;++i) {
    double omega2=std::max(0.0,ges.eigenvalues()(i));
    double omega=std::sqrt(omega2);
    Eigen::VectorXd phiFree=ges.eigenvectors().col(i); // mass-normalized by GeneralizedSelfAdjointEigenSolver
    Eigen::VectorXd phi12=Eigen::VectorXd::Zero(12); phi12.segment<6>(6)=phiFree;
    double Gamma=phi12.dot(Miota12);
    double T=2*3.14159265358979323846/omega;
    double Sa=kopruq::interpolateSpectrum(in.spectrum,T);
    double expectedDisp1 = (Gamma*Sa/omega2)*phi12(7); // node2 UY peak modal displacement

    // Match production's i-th mode by frequency proximity (ordering is ascending in both).
    EXPECT_NEAR(r.modes[i].frequency_hz, omega/(2*3.14159265358979323846), 1e-7*std::max(omega,1.0));
    EXPECT_NEAR(std::abs(r.modes[i].participation_factor), std::abs(Gamma), 1e-7*std::max(std::abs(Gamma),1e-6));
    // Displacement magnitude should match regardless of the eigenvector's arbitrary overall sign.
    double prodDisp1=0; for(const auto& n: r.modes[i].results.nodes) if(n.node_id==2) prodDisp1=n.displacement[1];
    EXPECT_NEAR(std::abs(prodDisp1), std::abs(expectedDisp1), 1e-6*std::max(std::abs(expectedDisp1),1e-9));
  }
}

TEST(ResponseSpectrum, CombinedResponseIsSrssOfModalResults) {
  auto m=cantilever(4.0,2);
  kopruq::ResponseSpectrumInput in; in.direction=0; in.num_modes=5;
  in.spectrum={{0.001,3.0},{0.5,6.0},{5.0,2.0}};
  auto r=kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),in);
  ASSERT_TRUE(r.status.success)<<r.status.message;

  for (const auto& cn : r.combined_nodes) {
    std::array<double,6> expected{};
    for (const auto& mc : r.modes) for (const auto& n : mc.results.nodes) if (n.node_id==cn.node_id)
      for (int k=0;k<6;++k) expected[k]+=n.displacement[k]*n.displacement[k];
    for (int k=0;k<6;++k) EXPECT_NEAR(cn.displacement[k], std::sqrt(expected[k]), 1e-9*std::max(std::sqrt(expected[k]),1e-300)) << "node "<<cn.node_id<<" disp "<<k;
  }
  for (const auto& ce : r.combined_elements) {
    std::array<double,12> expected{};
    for (const auto& mc : r.modes) for (const auto& e : mc.results.elements) if (e.element_id==ce.element_id)
      for (int k=0;k<12;++k) expected[k]+=e.global_end_force[k]*e.global_end_force[k];
    for (int k=0;k<12;++k) EXPECT_NEAR(ce.global_end_force[k], std::sqrt(expected[k]), 1e-9*std::max(std::sqrt(expected[k]),1e-300)) << "elem "<<ce.element_id<<" force "<<k;
  }
  // Every per-mode contribution is itself an exactly equilibrated static-equivalent result.
  for (const auto& mc : r.modes) {
    EXPECT_LT(mc.results.force_equilibrium_error, 1e-9);
    EXPECT_LT(mc.results.moment_equilibrium_error, 1e-9);
  }
}

TEST(ResponseSpectrum, ErrorHandling) {
  auto m=cantilever(4.0,1);
  kopruq::ResponseSpectrumInput bad_dir; bad_dir.direction=3; bad_dir.spectrum={{0.1,1.0},{1.0,1.0}};
  EXPECT_EQ(kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),bad_dir).status.code, kopruq::SolverStatusCode::InvalidModel);

  kopruq::ResponseSpectrumInput tooNarrow; tooNarrow.direction=1; tooNarrow.spectrum={{0.5,1.0},{0.6,1.0}}; // excludes this model's actual periods
  auto s=kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),tooNarrow);
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::InvalidModel);
  EXPECT_NE(s.status.message.find("outside"), std::string::npos) << s.status.message;

  kopruq::ResponseSpectrumInput noRestraint; noRestraint.direction=1; noRestraint.spectrum={{0.1,1.0},{10.0,1.0}};
  EXPECT_EQ(kopruq::ResponseSpectrumSolver::solve(m,{},noRestraint).status.code, kopruq::SolverStatusCode::NoRestraints);

  kopruq::ResponseSpectrumInput badSpectrum; badSpectrum.direction=1; badSpectrum.spectrum={{0.1,1.0}};
  EXPECT_EQ(kopruq::ResponseSpectrumSolver::solve(m,fixedNode1(),badSpectrum).status.code, kopruq::SolverStatusCode::InvalidModel);
}
