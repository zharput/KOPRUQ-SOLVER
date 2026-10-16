// Phase 3.2R3 — Linear static solver requalification.
//
// Independent verification of LinearStaticSolver: free/restrained DOF
// partition, reduced Kff and load-vector assembly, analytical cantilever
// displacements (axial, flexural, torsional, mixed), an independent 3D
// portal-frame dense reference solved without calling the production
// solver, singular/invalid-model handling, and determinism across
// multiple load cases. Expected values are derived from independently
// written beam-theory formulas and a fresh dense assembly, never from
// LinearStaticSolver/MatrixAssembler internals.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Real;
using Vec6 = Eigen::Matrix<Real,6,1>;

namespace {

kopruq::Model cantilever(kopruq::Id beamId, double L=4.0) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L,0,0}};
  m.materials={{1,210e6,.3}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{beamId,1,2,1,1,{0,1,0}}};
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixedNode1() {
  return {{1,{true,true,true,true,true,true}}};
}
const kopruq::NodeResults& nodeOf(const kopruq::AnalysisResults& r, kopruq::Id id) {
  for (const auto& n : r.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}

// Independent beam axes/local-stiffness/global-stiffness, written fresh
// (not calling kopruq::Beam3D) so the portal-frame reference does not
// depend on the production element formulation under test.
Eigen::Matrix3d indepAxes(const Eigen::Vector3d&a,const Eigen::Vector3d&b,const Eigen::Vector3d&ref) {
  Eigen::Vector3d x=(b-a).normalized();
  Eigen::Vector3d y=(ref-ref.dot(x)*x).normalized();
  Eigen::Vector3d z=x.cross(y).normalized();
  Eigen::Matrix3d R; R.row(0)=x.transpose(); R.row(1)=y.transpose(); R.row(2)=z.transpose();
  return R;
}
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
Eigen::MatrixXd indepGlobalK(const Eigen::Vector3d&a,const Eigen::Vector3d&b,const Eigen::Vector3d&ref,
                             double E,double G,double A,double Iy,double Iz,double J) {
  double L=(b-a).norm();
  Eigen::Matrix3d R=indepAxes(a,b,ref);
  Eigen::MatrixXd T=Eigen::MatrixXd::Zero(12,12);
  for (int i=0;i<4;++i) T.block<3,3>(3*i,3*i)=R;
  return T.transpose()*indepLocalK(L,E,G,A,Iy,Iz,J)*T;
}

} // namespace

TEST(LinearStaticRequalification, IndependentAxialCantileverDisplacement) {
  double L=4.0, E=210e6, A=.3, Fx=80.0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(31,L),fixedNode1(),{{2,(Vec6()<<Fx,0,0,0,0,0).finished()}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double expected_u = Fx*L/(E*A);
  EXPECT_NEAR(nodeOf(s.results,2).displacement[0], expected_u, 1e-9*std::abs(expected_u));
  // Global equilibrium (sum Fx = 0), independent of any internal sign convention.
  EXPECT_NEAR(nodeOf(s.results,1).reaction[0], -Fx, 1e-8);
}

TEST(LinearStaticRequalification, IndependentFlexuralCantileverDisplacement) {
  double L=4.0, E=210e6, Iz=.04, Fy=-50.0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(32,L),fixedNode1(),{{2,(Vec6()<<0,Fy,0,0,0,0).finished()}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double EI=E*Iz;
  double expected_v = Fy*L*L*L/(3*EI);
  double expected_theta = Fy*L*L/(2*EI);
  EXPECT_NEAR(nodeOf(s.results,2).displacement[1], expected_v, 1e-9*std::abs(expected_v));
  EXPECT_NEAR(nodeOf(s.results,2).displacement[5], expected_theta, 1e-9*std::abs(expected_theta));
  EXPECT_NEAR(nodeOf(s.results,1).reaction[1], -Fy, 1e-8);
  EXPECT_NEAR(nodeOf(s.results,1).reaction[5], -(Fy*L), 1e-6); // moment about node1: Mz_applied(0) + L*Fy
}

TEST(LinearStaticRequalification, IndependentTorsionalCantileverDisplacement) {
  double L=4.0, E=210e6, nu=.3, J=.01, Mx=25.0;
  double G=E/(2*(1+nu));
  auto s=kopruq::LinearStaticSolver::solve(cantilever(33,L),fixedNode1(),{{2,(Vec6()<<0,0,0,Mx,0,0).finished()}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double expected_phi = Mx*L/(G*J);
  EXPECT_NEAR(nodeOf(s.results,2).displacement[3], expected_phi, 1e-9*std::abs(expected_phi));
  EXPECT_NEAR(nodeOf(s.results,1).reaction[3], -Mx, 1e-8);
}

TEST(LinearStaticRequalification, IndependentMixedLoadSuperposition) {
  double L=4.0, E=210e6, nu=.3, A=.3, Iz=.04, J=.01;
  double G=E/(2*(1+nu));
  double Fx=80.0, Fy=-40.0, Mz=15.0, Mx=12.0;
  Vec6 load; load<<Fx,Fy,0,Mx,0,Mz;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(34,L),fixedNode1(),{{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double EI=E*Iz, GJ=G*J, EA=E*A;
  // Superposition of independently-standard cantilever cases: axial, torsion,
  // and (tip force + tip moment) in the same bending plane.
  double expected_u = Fx*L/EA;
  double expected_phi = Mx*L/GJ;
  double expected_v = Fy*L*L*L/(3*EI) + Mz*L*L/(2*EI);
  double expected_theta = Fy*L*L/(2*EI) + Mz*L/EI;
  const auto& tip = nodeOf(s.results,2);
  EXPECT_NEAR(tip.displacement[0], expected_u, 1e-9*std::abs(expected_u));
  EXPECT_NEAR(tip.displacement[3], expected_phi, 1e-9*std::abs(expected_phi));
  EXPECT_NEAR(tip.displacement[1], expected_v, 1e-9*std::abs(expected_v));
  EXPECT_NEAR(tip.displacement[5], expected_theta, 1e-9*std::abs(expected_theta));
  // Reactions from pure global statics (sum of forces/moments about node1 = 0),
  // independent of the element's internal sign convention.
  const auto& root = nodeOf(s.results,1);
  EXPECT_NEAR(root.reaction[0], -Fx, 1e-8);
  EXPECT_NEAR(root.reaction[1], -Fy, 1e-8);
  EXPECT_NEAR(root.reaction[3], -Mx, 1e-8);
  EXPECT_NEAR(root.reaction[5], -(Mz + L*Fy), 1e-6);
  // Linearity: the mixed case equals the sum of its single-component cases.
  auto sx=kopruq::LinearStaticSolver::solve(cantilever(35,L),fixedNode1(),{{2,(Vec6()<<Fx,0,0,0,0,0).finished()}});
  auto sy=kopruq::LinearStaticSolver::solve(cantilever(36,L),fixedNode1(),{{2,(Vec6()<<0,Fy,0,0,0,0).finished()}});
  auto srx=kopruq::LinearStaticSolver::solve(cantilever(37,L),fixedNode1(),{{2,(Vec6()<<0,0,0,Mx,0,0).finished()}});
  auto srz=kopruq::LinearStaticSolver::solve(cantilever(38,L),fixedNode1(),{{2,(Vec6()<<0,0,0,0,0,Mz).finished()}});
  ASSERT_TRUE(sx.status.success && sy.status.success && srx.status.success && srz.status.success);
  Eigen::VectorXd summed = sx.global + sy.global + srx.global + srz.global;
  EXPECT_LT((summed - s.global).cwiseAbs().maxCoeff(), 1e-9*s.global.cwiseAbs().maxCoeff());
}

TEST(LinearStaticRequalification, ReducedKffMatchesIndependentPartition) {
  // Independent 4x4 sparse system, not produced by MatrixAssembler.
  Eigen::SparseMatrix<Real> K(4,4);
  std::vector<Eigen::Triplet<Real>> t={
    {0,0,10},{1,1,20},{2,2,30},{3,3,40},
    {0,1,1},{1,0,1},{1,2,2},{2,1,2},{0,3,3},{3,0,3},{2,3,4},{3,2,4}
  };
  K.setFromTriplets(t.begin(),t.end());
  std::vector<std::size_t> free={0,2,3};
  auto Kff = kopruq::LinearStaticSolver::reduceKff(K, free);
  Eigen::MatrixXd dense(Kff);
  Eigen::MatrixXd expected(3,3);
  expected << 10,0,3,
               0,30,4,
               3,4,40;
  ASSERT_EQ(dense.rows(),3); ASSERT_EQ(dense.cols(),3);
  EXPECT_TRUE(dense.isApprox(expected,1e-12));
}

TEST(LinearStaticRequalification, AssembledLoadVectorAndNonFiniteRejection) {
  kopruq::Model m; m.nodes={{5,0,0,0},{2,1,0,0},{9,2,0,0}}; // sorted ids: 2,5,9
  kopruq::DofManager dm(m);
  Vec6 l1; l1<<1,2,3,4,5,6;
  Vec6 l3; l3<<-1,-2,-3,-4,-5,-6;
  std::vector<kopruq::NodalLoad> loads={{5,l1},{9,l3}};
  auto F = kopruq::LinearStaticSolver::assembleLoads(dm, loads);
  ASSERT_EQ(F.size(), 18);
  Eigen::VectorXd expected = Eigen::VectorXd::Zero(18);
  for (int i=0;i<6;++i) expected(6+i)=l1(i);  // node 5 -> second offset (ids sorted 2,5,9)
  for (int i=0;i<6;++i) expected(12+i)=l3(i); // node 9 -> third offset
  EXPECT_TRUE(F.isApprox(expected,1e-12));
  Vec6 bad; bad<<std::nan(""),0,0,0,0,0;
  EXPECT_THROW(kopruq::LinearStaticSolver::assembleLoads(dm, {{5,bad}}), kopruq::AssemblyError);
}

TEST(LinearStaticRequalification, NoRestraintsAndEmptyModelRejection) {
  auto s1 = kopruq::LinearStaticSolver::solve(cantilever(39), {}, {});
  EXPECT_FALSE(s1.status.success);
  EXPECT_EQ(s1.status.code, kopruq::SolverStatusCode::NoRestraints);

  kopruq::Model empty;
  auto s2 = kopruq::LinearStaticSolver::solve(empty, fixedNode1(), {});
  EXPECT_FALSE(s2.status.success);
  EXPECT_EQ(s2.status.code, kopruq::SolverStatusCode::InvalidModel);
}

TEST(LinearStaticRequalification, UnderconstrainedPinnedBeamIsSingular) {
  // Node 1 pinned (translations only; rotations free) and node 2 fully
  // free: the structure retains 3 unrestrained rigid-body rotation modes
  // about node 1, so Kff must be singular.
  auto m = cantilever(40);
  std::map<kopruq::Id,std::array<bool,6>> rs = {{1,{true,true,true,false,false,false}}};
  auto s = kopruq::LinearStaticSolver::solve(m, rs, {});
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::SingularSystem);
}

TEST(LinearStaticRequalification, IndependentPortalFrameDisplacementAndReactions) {
  double E=210e6, nu=.3, A=.3, Iy=.02, Iz=.04, J=.01;
  double G=E/(2*(1+nu));
  Eigen::Vector3d A1(0,0,0), B(0,0,3), C(4,0,3);

  // Independent dense global assembly (18 DOF: node1=A fixed, node2=B, node3=C),
  // partition and solve -- entirely independent of DofManager/MatrixAssembler/
  // LinearStaticSolver.
  Eigen::MatrixXd Kg = Eigen::MatrixXd::Zero(18,18);
  Eigen::MatrixXd k1 = indepGlobalK(A1,B,{1,0,0},E,G,A,Iy,Iz,J); // column A-B
  Eigen::MatrixXd k2 = indepGlobalK(B,C,{0,0,1},E,G,A,Iy,Iz,J);  // beam B-C
  int map1[12]={0,1,2,3,4,5,6,7,8,9,10,11};
  int map2[12]={6,7,8,9,10,11,12,13,14,15,16,17};
  for (int i=0;i<12;++i) for (int j=0;j<12;++j) {
    Kg(map1[i],map1[j]) += k1(i,j);
    Kg(map2[i],map2[j]) += k2(i,j);
  }
  Eigen::MatrixXd Kff = Kg.block(6,6,12,12);
  Eigen::MatrixXd Kfc = Kg.block(0,6,6,12); // fixed-rows, free-cols, for reactions
  Eigen::VectorXd Ff = Eigen::VectorXd::Zero(12);
  double Fy=-30.0;
  Ff(7) = Fy; // node C's UY, within the free block (C occupies free-local 6..11, UY is +1)
  Eigen::LDLT<Eigen::MatrixXd> ldlt(Kff);
  ASSERT_EQ(ldlt.info(), Eigen::Success);
  Eigen::VectorXd uFree = ldlt.solve(Ff);
  Eigen::VectorXd rFixed = Kfc*uFree; // F_fixed = 0, so R = Kfc*uFree

  // Production pipeline: full model -> solve.
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,0,0,3},{3,4,0,3}};
  m.materials={{1,E,nu}};
  m.sections={{1,A,Iy,Iz,J}};
  m.beams={{1,1,2,1,1,{1,0,0}},{2,2,3,1,1,{0,0,1}}};
  auto s=kopruq::LinearStaticSolver::solve(m, fixedNode1(), {{3,(Vec6()<<0,Fy,0,0,0,0).finished()}});
  ASSERT_TRUE(s.status.success)<<s.status.message;

  double scale = uFree.cwiseAbs().maxCoeff();
  const auto& nb = nodeOf(s.results,2);
  const auto& nc = nodeOf(s.results,3);
  for (int i=0;i<6;++i) EXPECT_NEAR(nb.displacement[i], uFree(i), 1e-9*scale) << "node B dof "<<i;
  for (int i=0;i<6;++i) EXPECT_NEAR(nc.displacement[i], uFree(6+i), 1e-9*scale) << "node C dof "<<i;
  double rscale = rFixed.cwiseAbs().maxCoeff();
  const auto& na = nodeOf(s.results,1);
  for (int i=0;i<6;++i) EXPECT_NEAR(na.reaction[i], rFixed(i), 1e-8*(rscale>0?rscale:1)) << "node A reaction "<<i;
}

TEST(LinearStaticRequalification, MultipleLoadCasesAreDeterministicAndEquilibrated) {
  auto m = cantilever(41);
  auto rs = fixedNode1();
  Vec6 loadCaseA; loadCaseA<<0,-60,0,0,0,0;
  Vec6 loadCaseB; loadCaseB<<40,0,0,0,0,20;

  auto s1 = kopruq::LinearStaticSolver::solve(m, rs, {{2,loadCaseA}});
  auto s1repeat = kopruq::LinearStaticSolver::solve(m, rs, {{2,loadCaseA}});
  auto s2 = kopruq::LinearStaticSolver::solve(m, rs, {{2,loadCaseB}});
  ASSERT_TRUE(s1.status.success && s1repeat.status.success && s2.status.success);

  // Deterministic: identical inputs give bit-identical outputs.
  ASSERT_EQ(s1.global.size(), s1repeat.global.size());
  for (Eigen::Index i=0;i<s1.global.size();++i) EXPECT_EQ(s1.global(i), s1repeat.global(i));

  // Distinct load cases give distinct, each internally equilibrated results.
  EXPECT_GT((s1.global-s2.global).cwiseAbs().maxCoeff(), 1e-9);
  EXPECT_LT(s1.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(s1.results.moment_equilibrium_error, 1e-9);
  EXPECT_LT(s2.results.force_equilibrium_error, 1e-9);
  EXPECT_LT(s2.results.moment_equilibrium_error, 1e-9);
}
