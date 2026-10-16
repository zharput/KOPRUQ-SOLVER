// Phase 3.2R4 — Reaction, element-force and energy recovery requalification.
//
// R3 (tests/unit/linear_static_solver_requalification_tests.cpp) already
// independently verified nodal displacements and reactions via global
// statics for axis-aligned cantilevers and a 3D portal frame. This suite
// covers what R3 did not: strain-energy consistency (strain_energy /
// element_strain_energy / energy_error, which were declared in
// AnalysisResults but never computed before this milestone), an inclined
// (non-axis-aligned) element's end-force and energy recovery, and
// per-element local/global end-force and equilibrium checks across a
// multi-element frame, using independently-written rotation/stiffness
// helpers that never call kopruq::Beam3D/MatrixAssembler.
//
// Recovery not repeating the solver factorization was verified by
// inspection, not by test: kopruq::LinearStaticSolver::recover() contains
// no Eigen::SimplicialLDLT construction, compute(), or solve() call; the
// single factorization happens once in solve() before recover() runs.
#include <gtest/gtest.h>
#include <Eigen/Dense>
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
const kopruq::ElementResults& elementOf(const kopruq::AnalysisResults& r, kopruq::Id id) {
  for (const auto& e : r.elements) if (e.element_id == id) return e;
  throw std::runtime_error("element not found");
}

// Independent beam axes/local-stiffness, written fresh for this file (not
// calling kopruq::Beam3D), so the per-element recovery reference below does
// not depend on the production element formulation under test.
Eigen::Matrix3d indepR(const Eigen::Vector3d&a,const Eigen::Vector3d&b,const Eigen::Vector3d&ref) {
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
Eigen::Matrix<Real,12,1> toVec12(const kopruq::NodeResults& a, const kopruq::NodeResults& b) {
  Eigen::Matrix<Real,12,1> u;
  for (int i=0;i<6;++i) { u(i)=a.displacement[i]; u(6+i)=b.displacement[i]; }
  return u;
}

} // namespace

TEST(RecoveryRequalification, IndependentAxialStrainEnergyConsistency) {
  double L=4.0, E=210e6, A=.3, Fx=100.0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(51,L),fixedNode1(),{{2,(Vec6()<<Fx,0,0,0,0,0).finished()}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double u = Fx*L/(E*A);
  double expected_energy = 0.5*Fx*u; // Clapeyron's theorem: external work = strain energy
  EXPECT_NEAR(s.results.strain_energy, expected_energy, 1e-9*expected_energy);
  EXPECT_NEAR(s.results.element_strain_energy, expected_energy, 1e-9*expected_energy);
  EXPECT_LT(s.results.energy_error, 1e-10);
  ASSERT_EQ(s.results.elements.size(), 1u);
  EXPECT_NEAR(elementOf(s.results,51).strain_energy, expected_energy, 1e-9*expected_energy);
}

TEST(RecoveryRequalification, IndependentCombinedLoadStrainEnergyConsistency) {
  double L=4.0, E=210e6, nu=.3, A=.3, Iz=.04, J=.01;
  double G=E/(2*(1+nu));
  double Fx=80.0, Fy=-40.0, Mz=15.0, Mx=12.0;
  Vec6 load; load<<Fx,Fy,0,Mx,0,Mz;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(52,L),fixedNode1(),{{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  double EA=E*A, GJ=G*J, EI=E*Iz;
  double u = Fx*L/EA;
  double phi = Mx*L/GJ;
  double v = Fy*L*L*L/(3*EI) + Mz*L*L/(2*EI);
  double theta = Fy*L*L/(2*EI) + Mz*L/EI;
  // Clapeyron's theorem for simultaneous loads: W = 0.5 * sum(load_i * displacement_i).
  double expected_energy = 0.5*(Fx*u + Fy*v + Mx*phi + Mz*theta);
  EXPECT_NEAR(s.results.strain_energy, expected_energy, 1e-9*std::abs(expected_energy));
  EXPECT_NEAR(s.results.element_strain_energy, expected_energy, 1e-9*std::abs(expected_energy));
  EXPECT_LT(s.results.energy_error, 1e-10);
}

TEST(RecoveryRequalification, IndependentInclinedElementRecovery) {
  // Beam not aligned with any global axis: node1=(0,0,0), node2=(3,4,0), L=5.
  double E=210e6, A=.3, P=90.0;
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,3,4,0}};
  m.materials={{1,E,.3}};
  m.sections={{1,A,.02,.04,.01}};
  m.beams={{61,1,2,1,1,{0,0,1}}};
  Eigen::Vector3d xDir(0.6,0.8,0.0); // (node2-node1)/L, independently computed
  Vec6 load; load<<P*xDir.x(),P*xDir.y(),0,0,0,0; // applied purely along the beam's own axis
  auto s=kopruq::LinearStaticSolver::solve(m, fixedNode1(), {{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;

  double L=5.0, u=P*L/(E*A);
  const auto& n2 = nodeOf(s.results,2);
  // A purely axial applied force along the beam axis excites only the axial
  // local DOF; the global displacement is u * xDir (no bending/torsion).
  EXPECT_NEAR(n2.displacement[0], u*xDir.x(), 1e-9*u);
  EXPECT_NEAR(n2.displacement[1], u*xDir.y(), 1e-9*u);
  EXPECT_NEAR(n2.displacement[2], 0.0, 1e-9*u);
  for (int i=3;i<6;++i) EXPECT_NEAR(n2.displacement[i], 0.0, 1e-9*std::max(1.0,u));

  // Global equilibrium: the force's line of action passes through node1
  // (collinear with the beam axis), so the reaction moment is exactly zero.
  const auto& n1 = nodeOf(s.results,1);
  EXPECT_NEAR(n1.reaction[0], -P*xDir.x(), 1e-8);
  EXPECT_NEAR(n1.reaction[1], -P*xDir.y(), 1e-8);
  for (int i=3;i<6;++i) EXPECT_NEAR(n1.reaction[i], 0.0, 1e-8);

  double expected_energy = 0.5*P*u;
  EXPECT_NEAR(s.results.strain_energy, expected_energy, 1e-9*expected_energy);
  EXPECT_NEAR(elementOf(s.results,61).strain_energy, expected_energy, 1e-9*expected_energy);
  EXPECT_LT(s.results.energy_error, 1e-10);
}

TEST(RecoveryRequalification, IndependentPortalFrameMultiElementRecovery) {
  double E=210e6, nu=.3, A=.3, Iy=.02, Iz=.04, J=.01;
  double G=E/(2*(1+nu));
  Eigen::Vector3d nA(0,0,0), nB(0,0,3), nC(4,0,3);
  Eigen::Vector3d refAB(1,0,0), refBC(0,0,1);

  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,0,0,3},{3,4,0,3}};
  m.materials={{1,E,nu}};
  m.sections={{1,A,Iy,Iz,J}};
  m.beams={{71,1,2,1,1,refAB},{72,2,3,1,1,refBC}};
  Vec6 load; load<<25,-35,0,0,0,8;
  auto s=kopruq::LinearStaticSolver::solve(m, fixedNode1(), {{3,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;

  const auto& rn1=nodeOf(s.results,1); const auto& rn2=nodeOf(s.results,2); const auto& rn3=nodeOf(s.results,3);

  struct ElemSpec{kopruq::Id id;Eigen::Vector3d a,b,ref;const kopruq::NodeResults*na,*nb;};
  std::vector<ElemSpec> elems={
    {71,nA,nB,refAB,&rn1,&rn2},
    {72,nB,nC,refBC,&rn2,&rn3},
  };
  double totalIndepEnergy=0.0;
  for (const auto& el : elems) {
    double L=(el.b-el.a).norm();
    Eigen::Matrix3d R=indepR(el.a,el.b,el.ref);
    Eigen::MatrixXd T=Eigen::MatrixXd::Zero(12,12);
    for (int i=0;i<4;++i) T.block<3,3>(3*i,3*i)=R;
    Eigen::MatrixXd kl=indepLocalK(L,E,G,A,Iy,Iz,J);
    Eigen::Matrix<Real,12,1> u=toVec12(*el.na,*el.nb);
    Eigen::VectorXd ul=T*u, fl=kl*ul, fg=T.transpose()*fl;

    const auto& er = elementOf(s.results, el.id);
    double scale = fl.cwiseAbs().maxCoeff(); scale = scale>0?scale:1;
    for (int i=0;i<12;++i) {
      EXPECT_NEAR(er.local_end_force[i], fl(i), 1e-9*scale) << "elem "<<el.id<<" local "<<i;
      EXPECT_NEAR(er.global_end_force[i], fg(i), 1e-9*scale) << "elem "<<el.id<<" global "<<i;
    }
    // Element-level equilibrium must hold independently for every element
    // in a multi-element structure, not only in a single-element model.
    EXPECT_LT(er.normalized_force_error, 1e-9) << "elem "<<el.id;
    EXPECT_LT(er.normalized_moment_error, 1e-9) << "elem "<<el.id;

    double energy = 0.5*ul.dot(fl);
    EXPECT_NEAR(er.strain_energy, energy, 1e-9*std::abs(energy)) << "elem "<<el.id;
    totalIndepEnergy += energy;
  }

  Eigen::Vector3d r(nC.x(),nC.y(),nC.z());
  double externalWork = 0.5*(load(0)*rn3.displacement[0]+load(1)*rn3.displacement[1]+load(5)*rn3.displacement[5]);
  EXPECT_NEAR(s.results.element_strain_energy, totalIndepEnergy, 1e-9*std::abs(totalIndepEnergy));
  EXPECT_NEAR(s.results.strain_energy, externalWork, 1e-9*std::abs(externalWork));
  EXPECT_NEAR(totalIndepEnergy, externalWork, 1e-8*std::abs(externalWork));
  EXPECT_LT(s.results.energy_error, 1e-9);
  (void)rn1; (void)r;
}
