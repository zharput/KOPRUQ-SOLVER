// Phase 6.1 — modal (free-vibration) analysis.
//
// Independent references: the classical closed-form consistent mass matrix
// (Przemieniecki / Clough & Penzien 420-denominator formula, with the same
// theta_y = -dw/dx sign convention already established for stiffness and
// loads) checked against the production quadrature; a dense generalized
// eigenproblem built and solved independently of ModalSolver for a single
// cantilever element; the Euler-Bernoulli continuum cantilever eigenvalue
// constant (beta1*L)^2 = 3.5160 (Blevins, "Formulas for Natural Frequency
// and Mode Shape") as a mesh-convergence check; a from-scratch Guyan mass
// reduction for a released element; and mathematical properties (rigid-body
// zero modes, K*phi = omega^2*M*phi residual, mass-orthonormality) that hold
// for any correct assembly, not only the production one.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <cmath>
#include "kopruq/solvers/ModalSolver.hpp"

using kopruq::LoadAxes;
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

// Independent stiffness (same formula verified in beam3d_requalification_tests).
Mat12 independentLocalK(double L) {
  Mat12 k=Mat12::Zero(); double ea=E*A/L, gj=G*J/L;
  double ay=12*E*IZ/(L*L*L), by=6*E*IZ/(L*L), cy=4*E*IZ/L, dy=2*E*IZ/L;
  double az=12*E*IY/(L*L*L), bz=6*E*IY/(L*L), cz=4*E*IY/L, dz=2*E*IY/L;
  k(0,0)=k(6,6)=ea;k(0,6)=k(6,0)=-ea;k(3,3)=k(9,9)=gj;k(3,9)=k(9,3)=-gj;
  k(1,1)=k(7,7)=ay;k(1,7)=k(7,1)=-ay;k(1,5)=k(5,1)=by;k(1,11)=k(11,1)=by;k(7,5)=k(5,7)=-by;k(7,11)=k(11,7)=-by;k(5,5)=k(11,11)=cy;k(5,11)=k(11,5)=dy;
  k(2,2)=k(8,8)=az;k(2,8)=k(8,2)=-az;k(2,4)=k(4,2)=-bz;k(2,10)=k(10,2)=-bz;k(8,4)=k(4,8)=bz;k(8,10)=k(10,8)=bz;k(4,4)=k(10,10)=cz;k(4,10)=k(10,4)=dz;
  return k;
}

// Independent consistent mass matrix: closed-form Przemieniecki/Clough&Penzien
// formula, not the production quadrature.
Mat12 independentLocalMass(double L) {
  Mat12 m=Mat12::Zero();
  double L2=L*L;
  // Axial, torsion: rho*length/6 * [[2,1],[1,2]].
  m(0,0)=m(6,6)=RHO_A*L/3; m(0,6)=m(6,0)=RHO_A*L/6;
  m(3,3)=m(9,9)=RHO_J*L/3; m(3,9)=m(9,3)=RHO_J*L/6;
  // y-bending (UY1,RZ1,UY2,RZ2), standard sign.
  double c=RHO_A*L/420;
  double My[4][4]={{156,22*L,54,-13*L},{22*L,4*L2,13*L,-3*L2},{54,13*L,156,-22*L},{-13*L,-3*L2,-22*L,4*L2}};
  int yi[4]={1,5,7,11};
  for(int a=0;a<4;++a)for(int b=0;b<4;++b)m(yi[a],yi[b])=c*My[a][b];
  // z-bending (UZ1,RY1,UZ2,RY2): same magnitudes, rotation rows/cols sign-flipped
  // because theta_y = -dw/dx (independently re-derived here, matching the
  // convention already established for stiffness/loads, not copied from them).
  double Mz[4][4]={{156,-22*L,54,13*L},{-22*L,4*L2,-13*L,-3*L2},{54,-13*L,156,22*L},{13*L,-3*L2,22*L,4*L2}};
  int zi[4]={2,4,8,10};
  for(int a=0;a<4;++a)for(int b=0;b<4;++b)m(zi[a],zi[b])=c*Mz[a][b];
  return m;
}

} // namespace

TEST(Modal, ConsistentMassMatrixMatchesClosedFormFormula) {
  double L=3.7;
  Mat12 expected=independentLocalMass(L);
  Mat12 actual=kopruq::consistentMassMatrix(L,RHO_A,RHO_J);
  EXPECT_LT((actual-expected).cwiseAbs().maxCoeff(), 1e-10*expected.cwiseAbs().maxCoeff());
  EXPECT_TRUE(actual.isApprox(actual.transpose(),1e-12));
  EXPECT_TRUE(actual.allFinite());
}

TEST(Modal, UnrestrainedElementHasExactlySixZeroModes) {
  kopruq::Model m; m.nodes={{1,0,0,0},{2,4,0,0}}; m.materials={{1,E,NU,UW}}; m.sections={{1,A,IY,IZ,J}};
  m.beams={{1,1,2,1,1,{0,1,0}}};
  // A single floating element has no restraints; ModalSolver requires at least
  // a pin. Verify the rigid-body-mode property directly on the assembled
  // (production) K, M instead: K*phi = 0 must hold for at least 6 independent
  // phi with phi^T*M*phi > 0 -- i.e. the generalized problem has 6 zero
  // eigenvalues. This is the standard rigid-body check, now against M.
  auto K=kopruq::MatrixAssembler::assemble(m), M=kopruq::MatrixAssembler::assembleMass(m);
  Eigen::MatrixXd Kd(K), Md(M);
  Eigen::LLT<Eigen::MatrixXd> chol(Md);
  ASSERT_EQ(chol.info(), Eigen::Success);
  Eigen::MatrixXd Linv=Eigen::MatrixXd::Identity(12,12); chol.matrixL().solveInPlace(Linv);
  Eigen::MatrixXd Asym=Linv*Kd*Linv.transpose(); Asym=0.5*(Asym+Asym.transpose());
  Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Asym);
  ASSERT_EQ(es.info(), Eigen::Success);
  double scale=es.eigenvalues().cwiseAbs().maxCoeff();
  int zero=0; for(int i=0;i<12;++i) if(std::abs(es.eigenvalues()(i))<1e-9*scale) ++zero;
  EXPECT_EQ(zero,6);
}

TEST(Modal, SingleElementCantileverMatchesIndependentDenseEigensolve) {
  double L=5.0;
  auto m=cantilever(L,1);
  auto r=kopruq::ModalSolver::solve(m,fixedNode1(),6);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  ASSERT_EQ(r.modes.size(),6u);

  Mat12 Kg=independentLocalK(L), Mg=independentLocalMass(L); // local = global axes
  Eigen::MatrixXd Kff=Kg.block(6,6,6,6), Mff=Mg.block(6,6,6,6);
  Eigen::GeneralizedSelfAdjointEigenSolver<Eigen::MatrixXd> ges(Kff,Mff);
  ASSERT_EQ(ges.info(),Eigen::Success);
  Eigen::VectorXd omega2=ges.eigenvalues().cwiseMax(0.0);
  std::vector<double> expected(omega2.data(),omega2.data()+omega2.size());
  std::sort(expected.begin(),expected.end());

  for (size_t i=0;i<6;++i) {
    double actual=r.modes[i].angular_frequency_rad_s*r.modes[i].angular_frequency_rad_s;
    EXPECT_NEAR(actual, expected[i], 1e-9*std::max(expected[i],1.0)) << "mode " << i;
    EXPECT_GT(r.modes[i].frequency_hz, 0.0);
  }
  // Frequencies are non-decreasing.
  for (size_t i=1;i<r.modes.size();++i) EXPECT_GE(r.modes[i].frequency_hz, r.modes[i-1].frequency_hz);
}

TEST(Modal, ModesSatisfyEigenproblemResidualAndMassOrthonormality) {
  auto m=cantilever(4.0,3);
  auto r=kopruq::ModalSolver::solve(m,fixedNode1(),6);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  Eigen::SparseMatrix<kopruq::Real> K=kopruq::MatrixAssembler::assemble(m), M=kopruq::MatrixAssembler::assembleMass(m);
  for (size_t i=0;i<r.modes.size();++i) {
    const auto& mi=r.modes[i];
    Eigen::VectorXd res=K*mi.shape - (mi.angular_frequency_rad_s*mi.angular_frequency_rad_s)*(M*mi.shape);
    double scale=std::max((K*mi.shape).norm(),1e-300);
    EXPECT_LT(res.norm()/scale, 1e-7) << "mode " << i << " eigenproblem residual";
    EXPECT_NEAR(mi.shape.dot(M*mi.shape), 1.0, 1e-9) << "mode " << i << " mass normalization";
    for (size_t j=i+1;j<r.modes.size();++j) {
      double cross=r.modes[j].shape.dot(M*mi.shape);
      EXPECT_NEAR(cross, 0.0, 1e-7) << "modes " << i << "," << j << " mass-orthogonality";
    }
  }
}

TEST(Modal, FirstFrequencyConvergesToEulerBernoulliContinuumConstant) {
  // Continuum cantilever: omega1 = (beta1*L)^2 * sqrt(EI / (m*L^4)), (beta1*L)^2 = 3.5160
  // (Blevins). m = mass per length = unit_weight/g * area.
  double L=6.0; int n=30;
  auto m=cantilever(L,n);
  auto r=kopruq::ModalSolver::solve(m,fixedNode1(),1);
  ASSERT_TRUE(r.status.success)<<r.status.message;
  double mass_per_length=UW/kopruq::g_standard*A;
  double constant=r.modes[0].angular_frequency_rad_s*L*L*std::sqrt(mass_per_length/(E*IZ));
  EXPECT_NEAR(constant, 3.5160, 0.02*3.5160) << "discretized first-mode constant vs continuum 3.5160";
}

TEST(Modal, GuyanMassCondensationMatchesIndependentReduction) {
  double L=4.0;
  Mat12 K=independentLocalK(L), Mm=independentLocalMass(L);
  kopruq::Releases rel{}; rel[5]=true; rel[11]=true;   // Mz released at both ends
  Eigen::MatrixXd Kc_prod=kopruq::condensedStiffness(K,rel), Mc_prod=kopruq::condensedMass(K,Mm,rel);

  std::vector<int> a,rr; for(int i=0;i<12;++i) (rel[i]?rr:a).push_back(i);
  Eigen::MatrixXd Krr(rr.size(),rr.size()),Kra(rr.size(),a.size()),Maa(a.size(),a.size()),Mar(a.size(),rr.size()),Mrr(rr.size(),rr.size());
  for(size_t i=0;i<rr.size();++i){for(size_t j=0;j<rr.size();++j)Krr(i,j)=K(rr[i],rr[j]);for(size_t j=0;j<a.size();++j)Kra(i,j)=K(rr[i],a[j]);}
  for(size_t i=0;i<a.size();++i){for(size_t j=0;j<a.size();++j)Maa(i,j)=Mm(a[i],a[j]);for(size_t j=0;j<rr.size();++j)Mar(i,j)=Mm(a[i],rr[j]);}
  for(size_t i=0;i<rr.size();++i)for(size_t j=0;j<rr.size();++j)Mrr(i,j)=Mm(rr[i],rr[j]);
  Eigen::MatrixXd T=Krr.ldlt().solve(Kra);
  Eigen::MatrixXd Mc_ref=Maa - Mar*T - T.transpose()*Mar.transpose() + T.transpose()*Mrr*T;

  Eigen::MatrixXd Mc_prod_aa(a.size(),a.size());
  for(size_t i=0;i<a.size();++i)for(size_t j=0;j<a.size();++j)Mc_prod_aa(i,j)=Mc_prod(a[i],a[j]);
  EXPECT_LT((Mc_prod_aa-Mc_ref).cwiseAbs().maxCoeff(), 1e-9*Mc_ref.cwiseAbs().maxCoeff());
  for(int i : rr) for(int j=0;j<12;++j) { EXPECT_EQ(Mc_prod(i,j),0.0); EXPECT_EQ(Mc_prod(j,i),0.0); }
  (void)Kc_prod;
}

TEST(Modal, ErrorHandling) {
  auto m=cantilever(4.0,1);
  EXPECT_EQ(kopruq::ModalSolver::solve(m,{},6).status.code, kopruq::SolverStatusCode::NoRestraints);

  kopruq::Model weightless=m; weightless.materials[0].unit_weight=0;
  auto s=kopruq::ModalSolver::solve(weightless,fixedNode1(),6);
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::SingularSystem);

  kopruq::Model empty;
  EXPECT_EQ(kopruq::ModalSolver::solve(empty,fixedNode1(),6).status.code, kopruq::SolverStatusCode::InvalidModel);

  auto r=kopruq::ModalSolver::solve(m,fixedNode1(),0);
  ASSERT_TRUE(r.status.success);
  EXPECT_EQ(r.modes.size(), r.free_dofs);
}
