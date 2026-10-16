// Phase 4.1 — member loads, self-weight, load cases and combinations.
//
// Expected values are independent of the production quadrature and shape
// functions: closed-form Euler-Bernoulli results (uniform and triangular
// loads, fixed-fixed and cantilever beams), a test-local virtual-work
// integral for partial trapezoidal loads, plain vector statics for
// reactions, and superposition for combinations.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::Real;
using kopruq::LoadAxes;
using kopruq::LoadCase;
using kopruq::MemberLoad;
using Vec6 = Eigen::Matrix<Real,6,1>;

namespace {

constexpr double E=210e6, NU=.3, A=.3, IY=.02, IZ=.04, J=.01;

kopruq::Model cantilever(double L=4.0, double unitWeight=0.0) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L,0,0}};
  m.materials={{1,E,NU,unitWeight}};
  m.sections={{1,A,IY,IZ,J}};
  m.beams={{11,1,2,1,1,{0,1,0}}};
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixedAt(std::initializer_list<kopruq::Id> ids) {
  std::map<kopruq::Id,std::array<bool,6>> r;
  for (auto id : ids) r[id]={true,true,true,true,true,true};
  return r;
}
const kopruq::NodeResults& nodeOf(const kopruq::AnalysisResults& r, kopruq::Id id) {
  for (const auto& n : r.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
const kopruq::ElementResults& elementOf(const kopruq::AnalysisResults& r, kopruq::Id id) {
  for (const auto& e : r.elements) if (e.element_id == id) return e;
  throw std::runtime_error("element not found");
}
kopruq::StaticSolution solveOne(const kopruq::Model& m, const std::map<kopruq::Id,std::array<bool,6>>& rs, const LoadCase& lc) {
  auto r=kopruq::LinearStaticSolver::solveCases(m,rs,{lc});
  return r.cases.front();
}
void expectRel(double actual, double expected, double tol, const char* what) {
  EXPECT_NEAR(actual, expected, tol*std::max(std::abs(expected),1e-300)) << what;
}

// Cantilever fixed at x=0, free at x=L, loaded by q(s) varying linearly from w1
// at s=xa to w2 at s=xb (transverse). Virtual work with the bending moment of
// the loads beyond each section, M(x) = integral_{max(x,xa)}^{xb} q(s)(s-x) ds:
//   tip deflection = (1/EI) integral M(x)(L-x) dx, tip rotation = (1/EI) integral M(x) dx.
// Each piece is a polynomial of degree <= 4, integrated exactly with 5-point Gauss.
struct TipResponse { double deflection, rotation; };
TipResponse cantileverTip(double L,double EI,double w1,double w2,double xa,double xb) {
  static const double gx[5]={-0.9061798459386640,-0.5384693101056831,0.0,0.5384693101056831,0.9061798459386640};
  static const double gw[5]={0.2369268850561891,0.4786286704993665,0.5688888888888889,0.4786286704993665,0.2369268850561891};
  auto q=[&](double s){return w1+(w2-w1)*(s-xa)/(xb-xa);};
  auto gauss=[&](double a,double b,auto f){double s=0;for(int i=0;i<5;++i)s+=gw[i]*f(0.5*(a+b)+0.5*(b-a)*gx[i]);return 0.5*(b-a)*s;};
  auto M=[&](double x){double lo=std::max(x,xa);if(lo>=xb)return 0.0;return gauss(lo,xb,[&](double s){return q(s)*(s-x);});};
  double defl=gauss(0,xa,[&](double x){return M(x)*(L-x);})+gauss(xa,xb,[&](double x){return M(x)*(L-x);});
  double rot=gauss(0,xa,M)+gauss(xa,xb,M);
  return {defl/EI,rot/EI};
}

} // namespace

TEST(MemberLoads, CantileverUniformLocalYMatchesClosedForm) {
  double L=4.0, q=-12.0, EI=E*IZ;
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,1,q,q,0,1}};
  auto s=solveOne(cantilever(L),fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  const auto& tip=nodeOf(s.results,2);
  expectRel(tip.displacement[1], q*L*L*L*L/(8*EI), 1e-9, "v_tip = qL^4/8EI");
  expectRel(tip.displacement[5], q*L*L*L/(6*EI), 1e-9, "theta_z = qL^3/6EI");
  // Reactions from statics: resultant qL at L/2.
  const auto& root=nodeOf(s.results,1);
  expectRel(root.reaction[1], -q*L, 1e-9, "Fy = -qL");
  expectRel(root.reaction[5], -q*L*L/2, 1e-9, "Mz = -qL^2/2");
  // End forces: the free end carries nothing; the root carries the reactions.
  const auto& e=elementOf(s.results,11);
  for (int i=6;i<12;++i) EXPECT_NEAR(e.global_end_force[i], 0.0, 1e-9*std::abs(q*L*L)) << "tip end force " << i;
  expectRel(e.global_end_force[1], -q*L, 1e-9, "root shear");
  expectRel(e.global_end_force[5], -q*L*L/2, 1e-9, "root moment");
  EXPECT_LT(e.normalized_force_error, 1e-12);
  EXPECT_LT(e.normalized_moment_error, 1e-12);
  EXPECT_LT(s.results.force_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.moment_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.energy_error, 1e-10);
}

TEST(MemberLoads, CantileverUniformLocalZUsesIyAndThetaYSign) {
  // theta_y = -dw/dx for a right-handed local frame.
  double L=4.0, q=7.5, EI=E*IY;
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,2,q,q,0,1}};
  auto s=solveOne(cantilever(L),fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  const auto& tip=nodeOf(s.results,2);
  expectRel(tip.displacement[2], q*L*L*L*L/(8*EI), 1e-9, "w_tip = qL^4/8EIy");
  expectRel(tip.displacement[4], -q*L*L*L/(6*EI), 1e-9, "theta_y = -qL^3/6EIy");
  const auto& root=nodeOf(s.results,1);
  expectRel(root.reaction[2], -q*L, 1e-9, "Fz = -qL");
  // Moment about node1 of the load: (x e_x) x (q e_z) = -x q e_y.
  expectRel(root.reaction[4], q*L*L/2, 1e-9, "My = +qL^2/2");
}

TEST(MemberLoads, CantileverAxialUniformLoad) {
  double L=4.0, p=20.0;
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,0,p,p,0,1}};
  auto s=solveOne(cantilever(L),fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  expectRel(nodeOf(s.results,2).displacement[0], p*L*L/(2*E*A), 1e-9, "u_tip = pL^2/2EA");
  expectRel(nodeOf(s.results,1).reaction[0], -p*L, 1e-9, "Fx = -pL");
}

TEST(MemberLoads, CantileverTriangularLoadMatchesClosedForm) {
  // q from w1 at the root to w2 at the tip = uniform w1 + triangle (0 -> w2-w1).
  double L=5.0, w1=-4.0, w2=-16.0, EI=E*IZ, d=w2-w1;
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,1,w1,w2,0,1}};
  auto s=solveOne(cantilever(L),fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  const auto& tip=nodeOf(s.results,2);
  expectRel(tip.displacement[1], w1*std::pow(L,4)/(8*EI)+11*d*std::pow(L,4)/(120*EI), 1e-9, "v_tip");
  expectRel(tip.displacement[5], w1*std::pow(L,3)/(6*EI)+d*std::pow(L,3)/(8*EI), 1e-9, "theta_tip");
  const auto& root=nodeOf(s.results,1);
  expectRel(root.reaction[1], -(w1+w2)*L/2, 1e-9, "Fy = -resultant");
  expectRel(root.reaction[5], -(w1*L*L/2+d*L*L/3), 1e-9, "Mz = -moment of load about root");
}

TEST(MemberLoads, CantileverPartialTrapezoidMatchesVirtualWork) {
  double L=6.0, w1=-3.0, w2=-9.0, a=0.25, b=0.8, EI=E*IZ;
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,1,w1,w2,a,b}};
  auto s=solveOne(cantilever(L),fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  auto ref=cantileverTip(L,EI,w1,w2,a*L,b*L);
  const auto& tip=nodeOf(s.results,2);
  expectRel(tip.displacement[1], ref.deflection, 1e-9, "v_tip vs virtual work");
  expectRel(tip.displacement[5], ref.rotation, 1e-9, "theta_tip vs virtual work");
  double len=(b-a)*L, resultant=(w1+w2)/2*len;
  double centroid=a*L+len*(w1+2*w2)/(3*(w1+w2));
  expectRel(nodeOf(s.results,1).reaction[1], -resultant, 1e-9, "Fy");
  expectRel(nodeOf(s.results,1).reaction[5], -resultant*centroid, 1e-9, "Mz");
}

TEST(MemberLoads, FixedFixedUniformTwoElementsMatchesClosedForm) {
  double L=8.0, q=-10.0, EI=E*IZ;
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L/2,0,0},{3,L,0,0}};
  m.materials={{1,E,NU}};
  m.sections={{1,A,IY,IZ,J}};
  m.beams={{21,1,2,1,1,{0,1,0}},{22,2,3,1,1,{0,1,0}}};
  LoadCase lc; lc.member={MemberLoad{21,LoadAxes::Local,1,q,q,0,1},MemberLoad{22,LoadAxes::Local,1,q,q,0,1}};
  auto s=solveOne(m,fixedAt({1,3}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  expectRel(nodeOf(s.results,2).displacement[1], q*std::pow(L,4)/(384*EI), 1e-9, "v_mid = qL^4/384EI");
  EXPECT_NEAR(nodeOf(s.results,2).displacement[5], 0.0, 1e-12);
  expectRel(nodeOf(s.results,1).reaction[1], -q*L/2, 1e-9, "left Fy");
  expectRel(nodeOf(s.results,3).reaction[1], -q*L/2, 1e-9, "right Fy");
  expectRel(nodeOf(s.results,1).reaction[5], -q*L*L/12, 1e-9, "left Mz = qL^2/12 hogging");
  expectRel(nodeOf(s.results,3).reaction[5], q*L*L/12, 1e-9, "right Mz");
  // Element 21 free body: zero shear and a qL^2/24 sagging moment at midspan.
  const auto& e=elementOf(s.results,21);
  EXPECT_NEAR(e.local_end_force[7], 0.0, 1e-9*std::abs(q*L));
  expectRel(e.local_end_force[11], -q*L*L/24, 1e-9, "midspan moment");
  expectRel(e.local_end_force[5], -q*L*L/12, 1e-9, "support moment");
}

TEST(MemberLoads, GlobalLoadOnInclinedMemberActsPerTrueLength) {
  // Member (0,0,0)->(3,0,4), L=5, uniform global -Z load per true length.
  double wz=-6.0, L=5.0;
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,3,0,4}};
  m.materials={{1,E,NU}};
  m.sections={{1,A,IY,IZ,J}};
  m.beams={{31,1,2,1,1,{0,1,0}}};
  LoadCase lc; lc.member={MemberLoad{31,LoadAxes::Global,2,wz,wz,0,1}};
  auto s=solveOne(m,fixedAt({1}),lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;

  // Statics: resultant wz*L (true length) at the midpoint (1.5, 0, 2).
  Eigen::Vector3d F(0,0,wz*L), rc(1.5,0,2), Mr=rc.cross(F);
  const auto& root=nodeOf(s.results,1);
  for (int i=0;i<3;++i) EXPECT_NEAR(root.reaction[i], -F(i), 1e-9*std::abs(wz*L)) << "force " << i;
  for (int i=0;i<3;++i) EXPECT_NEAR(root.reaction[3+i], -Mr(i), 1e-9*Mr.norm()) << "moment " << i;

  // Tip displacement: split the load into axial and transverse components.
  Eigen::Vector3d x(0.6,0,0.8), y(0,1,0), z=x.cross(y);
  double px=x.dot(Eigen::Vector3d(0,0,wz)), pz=z.dot(Eigen::Vector3d(0,0,wz));
  double u=px*L*L/(2*E*A), w=pz*std::pow(L,4)/(8*E*IY), thy=-pz*L*L*L/(6*E*IY);
  Eigen::Vector3d dg=u*x+w*z, rg=thy*y;
  const auto& tip=nodeOf(s.results,2);
  double ds=dg.norm(), rs=rg.norm();
  for (int i=0;i<3;++i) EXPECT_NEAR(tip.displacement[i], dg(i), 1e-9*ds) << "disp " << i;
  for (int i=0;i<3;++i) EXPECT_NEAR(tip.displacement[3+i], rg(i), 1e-9*rs) << "rot " << i;
}

TEST(MemberLoads, SelfWeightEqualsExplicitGlobalLoad) {
  double L=4.0, gamma=78.5, factor=1.35;
  auto m=cantilever(L,gamma);
  LoadCase sw; sw.self_weight=factor;
  LoadCase explicitLoad; double w=-gamma*A*factor;
  explicitLoad.member={MemberLoad{11,LoadAxes::Global,2,w,w,0,1}};
  auto r=kopruq::LinearStaticSolver::solveCases(m,fixedAt({1}),{sw,explicitLoad});
  ASSERT_TRUE(r.status.success)<<r.status.message;
  const auto& a=r.cases[0]; const auto& b=r.cases[1];
  double scale=b.global.cwiseAbs().maxCoeff();
  EXPECT_LT((a.global-b.global).cwiseAbs().maxCoeff(), 1e-12*scale);
  expectRel(nodeOf(a.results,1).reaction[2], gamma*A*L*factor, 1e-9, "Fz = total weight");
  // A weightless material produces no self-weight load.
  auto r0=kopruq::LinearStaticSolver::solveCases(cantilever(L,0.0),fixedAt({1}),{sw});
  ASSERT_TRUE(r0.status.success);
  EXPECT_EQ(r0.cases[0].global.cwiseAbs().maxCoeff(), 0.0);
}

TEST(MemberLoads, CombinationsEqualSuperpositionAndDirectSolve) {
  double L=4.0;
  auto m=cantilever(L,25.0);
  auto rs=fixedAt({1});
  LoadCase nodal; nodal.nodal={{2,(Vec6()<<10,-20,5,3,0,-4).finished()}};
  LoadCase member; member.member={MemberLoad{11,LoadAxes::Local,1,-6,-2,0.1,0.9},MemberLoad{11,LoadAxes::Global,0,4,4,0,1}};
  LoadCase weight; weight.self_weight=1.0;
  kopruq::LoadCombination uls{"ULS",{{0,1.35},{1,1.5},{2,1.0}}};
  auto r=kopruq::LinearStaticSolver::solveCases(m,rs,{nodal,member,weight},{uls});
  ASSERT_TRUE(r.status.success)<<r.status.message;
  ASSERT_EQ(r.combinations.size(),1u);
  const auto& c=r.combinations[0];
  ASSERT_TRUE(c.status.success)<<c.status.message;

  Eigen::VectorXd U=1.35*r.cases[0].global+1.5*r.cases[1].global+1.0*r.cases[2].global;
  Eigen::VectorXd R=1.35*r.cases[0].reactions+1.5*r.cases[1].reactions+1.0*r.cases[2].reactions;
  EXPECT_LT((c.global-U).cwiseAbs().maxCoeff(), 1e-12*U.cwiseAbs().maxCoeff());
  EXPECT_LT((c.reactions-R).cwiseAbs().maxCoeff(), 1e-9*R.cwiseAbs().maxCoeff());
  const auto& ec=elementOf(c.results,11);
  for (int i=0;i<12;++i) {
    double sum=1.35*elementOf(r.cases[0].results,11).global_end_force[i]+1.5*elementOf(r.cases[1].results,11).global_end_force[i]+elementOf(r.cases[2].results,11).global_end_force[i];
    EXPECT_NEAR(ec.global_end_force[i], sum, 1e-9*R.cwiseAbs().maxCoeff()) << "end force " << i;
  }

  // Direct solve of the same combined loads.
  LoadCase direct;
  direct.nodal={{2,(1.35*(Vec6()<<10,-20,5,3,0,-4).finished()).eval()}};
  direct.member={MemberLoad{11,LoadAxes::Local,1,-6*1.5,-2*1.5,0.1,0.9},MemberLoad{11,LoadAxes::Global,0,4*1.5,4*1.5,0,1}};
  direct.self_weight=1.0;
  auto d=solveOne(m,rs,direct);
  ASSERT_TRUE(d.status.success);
  EXPECT_LT((c.global-d.global).cwiseAbs().maxCoeff(), 1e-10*d.global.cwiseAbs().maxCoeff());
  EXPECT_LT(c.relative_residual, 1e-14);
  EXPECT_LT(c.results.force_equilibrium_error, 1e-12);
  EXPECT_LT(c.results.moment_equilibrium_error, 1e-12);
}

TEST(MemberLoads, FixedHoldsReactionsOnRestrainedDofsInAscendingOrder) {
  kopruq::Model m=cantilever();
  m.nodes.push_back({3,8,0,0});
  m.beams.push_back({12,2,3,1,1,{0,1,0}});
  // Node 1: UX, UY, UZ, RX restrained; node 3: UY, UZ restrained.
  std::map<kopruq::Id,std::array<bool,6>> rs={{1,{true,true,true,true,false,false}},{3,{false,true,true,false,false,false}}};
  LoadCase lc; lc.member={MemberLoad{11,LoadAxes::Local,1,-5,-5,0,1},MemberLoad{12,LoadAxes::Local,2,3,3,0,1}};
  auto s=solveOne(m,rs,lc);
  ASSERT_TRUE(s.status.success)<<s.status.message;
  // DOFs: node 1 -> 0..5, node 2 -> 6..11, node 3 -> 12..17.
  std::vector<int> expectedDofs={0,1,2,3,13,14};
  ASSERT_EQ(s.fixed.size(), expectedDofs.size());
  ASSERT_EQ(s.fixed_dofs, expectedDofs.size());
  for (size_t i=0;i<expectedDofs.size();++i) EXPECT_EQ(s.fixed[i], s.reactions(expectedDofs[i])) << "fixed " << i;
  EXPECT_NE(s.fixed[1], 0.0);
}

TEST(MemberLoads, InvalidMemberLoadsAndMaterialsAreRejected) {
  auto bad=[](MemberLoad l){LoadCase lc; lc.member={l}; return solveOne(cantilever(),fixedAt({1}),lc).status;};
  EXPECT_EQ(bad({99,LoadAxes::Local,1,1,1,0,1}).code, kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad({11,LoadAxes::Local,3,1,1,0,1}).code, kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad({11,LoadAxes::Local,1,1,1,0.6,0.6}).code, kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad({11,LoadAxes::Local,1,1,1,-0.1,0.5}).code, kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad({11,LoadAxes::Local,1,std::nan(""),1,0,1}).code, kopruq::SolverStatusCode::InvalidModel);
  LoadCase sw; sw.self_weight=1.0;
  EXPECT_EQ(solveOne(cantilever(4.0,-1.0),fixedAt({1}),sw).status.code, kopruq::SolverStatusCode::InvalidModel);
  kopruq::LoadCombination wrong{"bad",{{5,1.0}}};
  EXPECT_EQ(kopruq::LinearStaticSolver::solveCases(cantilever(),fixedAt({1}),{sw},{wrong}).status.code, kopruq::SolverStatusCode::InvalidModel);
}
