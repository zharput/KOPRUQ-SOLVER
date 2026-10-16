// Phase 4.3 — concentrated member forces/moments and distributed moments.
//
// Expected values: closed-form Euler-Bernoulli results by virtual work
// (cantilever under a point force, a point moment, a uniform distributed
// moment or torque; fixed-fixed beam reactions under a point force), plain
// vector statics for reactions, and equality with the equivalent nodal load
// when the point load sits on a node. None use the production shape functions.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>
#include "kopruq/io/ModelJson.hpp"

using kopruq::LoadAxes;
using kopruq::LoadCase;
using kopruq::MemberLoad;
using kopruq::MemberLoadKind;
using kopruq::MemberPointLoad;
using Vec6 = Eigen::Matrix<double,6,1>;

namespace {

constexpr double E=210e6, NU=.3, A=.3, IY=.02, IZ=.04, J=.01, G=E/(2*(1+NU)), L=5.0;
constexpr auto F=MemberLoadKind::Force;
constexpr auto M=MemberLoadKind::Moment;

kopruq::Model cantilever() {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L,0,0}};
  m.materials={{1,E,NU}};
  m.sections={{1,A,IY,IZ,J}};
  m.beams={{7,1,2,1,1,{0,1,0}}};      // local axes = global axes
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixed1() { return {{1,{true,true,true,true,true,true}}}; }
kopruq::StaticSolution solve(const kopruq::Model& m, const LoadCase& lc, std::map<kopruq::Id,std::array<bool,6>> rs=fixed1()) {
  return kopruq::LinearStaticSolver::solveCases(m,rs,{lc}).cases.front();
}
LoadCase point(MemberPointLoad p) { LoadCase lc; lc.point={p}; return lc; }
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, kopruq::Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
void rel(double actual, double expected, const char* what, double tol=1e-9) {
  EXPECT_NEAR(actual, expected, tol*std::max(std::abs(expected),1e-300)) << what;
}
void cleanEquilibrium(const kopruq::StaticSolution& s) {
  ASSERT_TRUE(s.status.success) << s.status.message;
  EXPECT_LT(s.results.force_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.moment_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.max_element_force_error, 1e-12);
  EXPECT_LT(s.results.max_element_moment_error, 1e-12);
  EXPECT_LT(s.results.energy_error, 1e-10);
}

} // namespace

TEST(PointLoads, CantileverPointForceLocalY) {
  double P=-30, a=3.0, EI=E*IZ;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,1,F,P,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[1], P*a*a*(3*L-a)/(6*EI), "v_tip = Pa^2(3L-a)/6EI");
  rel(nodeOf(s,2).displacement[5], P*a*a/(2*EI), "theta_z = Pa^2/2EI");
  rel(nodeOf(s,1).reaction[1], -P, "Fy");
  rel(nodeOf(s,1).reaction[5], -P*a, "Mz");
  // The free end carries nothing.
  for (int i=6;i<12;++i) EXPECT_NEAR(s.results.elements[0].global_end_force[i], 0.0, 1e-9*std::abs(P*a)) << i;
}

TEST(PointLoads, CantileverPointForceLocalZ) {
  double P=12, a=1.5, EI=E*IY;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,2,F,P,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[2], P*a*a*(3*L-a)/(6*EI), "w_tip");
  rel(nodeOf(s,2).displacement[4], -P*a*a/(2*EI), "theta_y = -dw/dx");
  rel(nodeOf(s,1).reaction[4], P*a, "My: (a e_x) x (P e_z) = -aP e_y");
}

TEST(PointLoads, CantileverPointForceAxial) {
  double P=50, a=2.0;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,0,F,P,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[0], P*a/(E*A), "u_tip = Pa/EA");
  rel(nodeOf(s,1).reaction[0], -P, "Fx");
}

TEST(PointLoads, CantileverPointMomentAboutZ) {
  double Mz=40, a=2.0, EI=E*IZ;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,2,M,Mz,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[5], Mz*a/EI, "theta_z = Ma/EI");
  rel(nodeOf(s,2).displacement[1], Mz*a*(L-a/2)/EI, "v_tip = Ma(L-a/2)/EI");
  rel(nodeOf(s,1).reaction[5], -Mz, "Mz reaction");
  EXPECT_NEAR(nodeOf(s,1).reaction[1], 0.0, 1e-9*Mz);
}

TEST(PointLoads, CantileverPointMomentAboutY) {
  double My=-25, a=4.0, EI=E*IY;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,1,M,My,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[4], My*a/EI, "theta_y = Ma/EIy");
  rel(nodeOf(s,2).displacement[2], -My*a*(L-a/2)/EI, "w_tip = -Ma(L-a/2)/EIy");
  rel(nodeOf(s,1).reaction[4], -My, "My reaction");
}

TEST(PointLoads, CantileverPointTorque) {
  double T=18, a=3.5;
  auto s=solve(cantilever(),point({7,LoadAxes::Local,0,M,T,a/L}));
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[3], T*a/(G*J), "phi_tip = Ta/GJ");
  rel(nodeOf(s,1).reaction[3], -T, "Mx reaction");
}

TEST(PointLoads, CantileverUniformDistributedTorqueAndMoment) {
  double mt=6, mz=-4;
  LoadCase lc;
  lc.member={MemberLoad{7,LoadAxes::Local,0,mt,mt,0,1,false,M},MemberLoad{7,LoadAxes::Local,2,mz,mz,0,1,false,M}};
  auto s=solve(cantilever(),lc);
  cleanEquilibrium(s);
  rel(nodeOf(s,2).displacement[3], mt*L*L/(2*G*J), "phi_tip = mL^2/2GJ");
  rel(nodeOf(s,2).displacement[5], mz*L*L/(2*E*IZ), "theta_z = mL^2/2EI");
  rel(nodeOf(s,2).displacement[1], mz*L*L*L/(3*E*IZ), "v_tip = mL^3/3EI");
  rel(nodeOf(s,1).reaction[3], -mt*L, "Mx");
  rel(nodeOf(s,1).reaction[5], -mz*L, "Mz");
  EXPECT_NEAR(nodeOf(s,1).reaction[1], 0.0, 1e-9*std::abs(mz*L));
}

TEST(PointLoads, PartialDistributedMomentMatchesVirtualWork) {
  // m over [a,b]: M(x) = m (b - max(x,a)) for x < b; theta_tip = (1/EI) integral M dx.
  double m=7, a=1.0, b=3.5, EI=E*IZ;
  LoadCase lc; lc.member={MemberLoad{7,LoadAxes::Local,2,m,m,a/L,b/L,false,M}};
  auto s=solve(cantilever(),lc);
  cleanEquilibrium(s);
  double theta=m*(a*(b-a)+(b-a)*(b-a)/2)/EI;
  // v_tip = (1/EI) integral M(x)(L-x) dx, split at a.
  double v=m*((b-a)*(L*a-a*a/2) + ((b-a)*(b-a)*(3*L-2*a-b))/6)/EI;
  rel(nodeOf(s,2).displacement[5], theta, "theta_tip");
  rel(nodeOf(s,2).displacement[1], v, "v_tip");
  rel(nodeOf(s,1).reaction[5], -m*(b-a), "Mz");
}

TEST(PointLoads, PointLoadOnANodeEqualsNodalLoad) {
  auto m=cantilever();
  for (auto [dir,kind,val] : {std::tuple{1,F,-30.0},std::tuple{2,F,12.0},std::tuple{0,F,7.0},std::tuple{2,M,9.0},std::tuple{1,M,-4.0},std::tuple{0,M,3.0}}) {
    auto s=solve(m,point({7,LoadAxes::Local,dir,kind,val,1.0}));
    Vec6 v=Vec6::Zero(); v(dir+(kind==M?3:0))=val;
    LoadCase nodal; nodal.nodal={{2,v}};
    auto n=solve(m,nodal);
    ASSERT_TRUE(s.status.success && n.status.success);
    EXPECT_LT((s.global-n.global).cwiseAbs().maxCoeff(), 1e-12*n.global.cwiseAbs().maxCoeff()) << dir << " " << (kind==M);
    // At node 1 the load goes straight into the support: no deformation.
    auto r=solve(m,point({7,LoadAxes::Local,dir,kind,val,0.0}));
    ASSERT_TRUE(r.status.success);
    EXPECT_EQ(r.global.cwiseAbs().maxCoeff(), 0.0);
    rel(nodeOf(r,1).reaction[dir+(kind==M?3:0)], -val, "support takes the load");
  }
}

TEST(PointLoads, FixedFixedBeamReactionsUnderPointForce) {
  // Two elements over L2 = 8; load at a = 2 inside element 1.
  double P=-40, L2=8, a=2, b=L2-a;
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,L2/2,0,0},{3,L2,0,0}};
  m.materials={{1,E,NU}}; m.sections={{1,A,IY,IZ,J}};
  m.beams={{1,1,2,1,1,{0,1,0}},{2,2,3,1,1,{0,1,0}}};
  auto s=solve(m,point({1,LoadAxes::Local,1,F,P,a/(L2/2)}),{{1,{true,true,true,true,true,true}},{3,{true,true,true,true,true,true}}});
  cleanEquilibrium(s);
  rel(nodeOf(s,1).reaction[1], -P*b*b*(3*a+b)/(L2*L2*L2), "R_A");
  rel(nodeOf(s,3).reaction[1], -P*a*a*(a+3*b)/(L2*L2*L2), "R_B");
  rel(nodeOf(s,1).reaction[5], -P*a*b*b/(L2*L2), "M_A = Pab^2/L^2 (hogging)");
  rel(nodeOf(s,3).reaction[5], P*a*a*b/(L2*L2), "M_B = Pa^2b/L^2");
}

TEST(PointLoads, GlobalPointForceOnInclinedMemberStatics) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,3,0,4}};
  m.materials={{1,E,NU}}; m.sections={{1,A,IY,IZ,J}};
  m.beams={{3,1,2,1,1,{0,1,0}}};
  double P=-25, t=0.4;
  auto s=solve(m,point({3,LoadAxes::Global,2,F,P,t}));
  cleanEquilibrium(s);
  Eigen::Vector3d Fv(0,0,P), r=t*Eigen::Vector3d(3,0,4), Mv=r.cross(Fv);
  for (int i=0;i<3;++i) EXPECT_NEAR(nodeOf(s,1).reaction[i], -Fv(i), 1e-9*std::abs(P)) << i;
  for (int i=0;i<3;++i) EXPECT_NEAR(nodeOf(s,1).reaction[3+i], -Mv(i), 1e-9*Mv.norm()) << i;
}

TEST(PointLoads, CombinationsScalePointLoads) {
  auto m=cantilever();
  LoadCase a; a.point={{7,LoadAxes::Local,1,F,-10,0.3}};
  LoadCase b; b.point={{7,LoadAxes::Local,2,M,5,0.8}}; b.member={MemberLoad{7,LoadAxes::Local,0,2,2,0,1,false,M}};
  auto r=kopruq::LinearStaticSolver::solveCases(m,fixed1(),{a,b},{{"C",{{0,1.35},{1,1.5}}}});
  ASSERT_TRUE(r.status.success) << r.status.message;
  Eigen::VectorXd U=1.35*r.cases[0].global+1.5*r.cases[1].global;
  EXPECT_LT((r.combinations[0].global-U).cwiseAbs().maxCoeff(), 1e-12*U.cwiseAbs().maxCoeff());
  Eigen::VectorXd R=1.35*r.cases[0].reactions+1.5*r.cases[1].reactions;
  EXPECT_LT((r.combinations[0].reactions-R).cwiseAbs().maxCoeff(), 1e-9*R.cwiseAbs().maxCoeff());
  EXPECT_LT(r.combinations[0].results.moment_equilibrium_error, 1e-12);
}

TEST(PointLoads, InvalidPointLoadsAndProjectedMomentsAreRejected) {
  auto bad=[](LoadCase lc){ return solve(cantilever(),lc).status.code; };
  EXPECT_EQ(bad(point({9,LoadAxes::Local,1,F,1,0.5})), kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad(point({7,LoadAxes::Local,3,F,1,0.5})), kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad(point({7,LoadAxes::Local,1,F,1,1.5})), kopruq::SolverStatusCode::InvalidModel);
  EXPECT_EQ(bad(point({7,LoadAxes::Local,1,F,std::nan(""),0.5})), kopruq::SolverStatusCode::InvalidModel);
  LoadCase pm; pm.member={MemberLoad{7,LoadAxes::Global,2,1,1,0,1,true,M}};
  EXPECT_EQ(bad(pm), kopruq::SolverStatusCode::InvalidModel);
}

TEST(PointLoads, JsonReadsWritesAndValidatesPointLoads) {
  using kopruq::io::json;
  json doc=json::parse(R"({
    "schema": "kopruq-model", "schema_version": 1,
    "nodes": [{"id": 1, "x": 0, "y": 0, "z": 0}, {"id": 2, "x": 5, "y": 0, "z": 0}],
    "materials": [{"id": 1, "elastic_modulus": 210e6, "poisson_ratio": 0.3}],
    "sections": [{"id": 1, "area": 0.3, "Iy": 0.02, "Iz": 0.04, "J": 0.01}],
    "beams": [{"id": 7, "nodes": [1, 2], "material": 1, "section": 1}],
    "supports": [{"node": 1, "restraints": [true, true, true, true, true, true]}],
    "load_cases": [{"name": "P",
      "point": [{"element": 7, "axes": "local", "direction": "y", "value": -30, "position": 0.6},
                {"element": 7, "axes": "global", "direction": "z", "kind": "moment", "value": 40, "position": 0.4}],
      "member": [{"element": 7, "axes": "local", "direction": "x", "kind": "moment", "w1": 6}]}]
  })");
  auto in=kopruq::io::readModel(doc);
  ASSERT_EQ(in.load_cases[0].point.size(), 2u);
  const auto& p0=in.load_cases[0].point[0];
  EXPECT_EQ(p0.kind, F); EXPECT_EQ(p0.direction, 1); EXPECT_EQ(p0.value, -30.0); EXPECT_EQ(p0.position, 0.6);
  EXPECT_EQ(in.load_cases[0].point[1].kind, M); EXPECT_EQ(in.load_cases[0].point[1].axes, LoadAxes::Global);
  EXPECT_EQ(in.load_cases[0].member[0].kind, M);
  // Round trip.
  EXPECT_EQ(kopruq::io::writeModel(kopruq::io::readModelText(kopruq::io::writeModel(in).dump())), kopruq::io::writeModel(in));
  // JSON solve equals the C++ API solve of the same loads.
  auto viaJson=kopruq::LinearStaticSolver::solveCases(in.model,in.restraints,in.load_cases).cases[0];
  LoadCase lc; lc.point={{7,LoadAxes::Local,1,F,-30,0.6},{7,LoadAxes::Global,2,M,40,0.4}}; lc.member={MemberLoad{7,LoadAxes::Local,0,6,6,0,1,false,M}};
  auto direct=solve(cantilever(),lc);
  ASSERT_TRUE(viaJson.status.success && direct.status.success);
  EXPECT_EQ((viaJson.global-direct.global).cwiseAbs().maxCoeff(), 0.0);
  // Validation with paths.
  auto issues=[&](auto mutate){ json d=doc; mutate(d); try{ kopruq::io::readModel(d); }catch(const kopruq::io::ModelInputError& e){ return e.issues; } return std::vector<kopruq::io::InputIssue>{}; };
  auto has=[](const std::vector<kopruq::io::InputIssue>& v,const std::string& path,const std::string& text){
    return std::any_of(v.begin(),v.end(),[&](const auto& i){ return i.path==path && i.message.find(text)!=std::string::npos; }); };
  EXPECT_TRUE(has(issues([](json& d){ d["load_cases"][0]["point"][0]["position"]=1.5; }), "$.load_cases[0].point[0].position", "0 <= position <= 1"));
  EXPECT_TRUE(has(issues([](json& d){ d["load_cases"][0]["point"][0]["kind"]="torque"; }), "$.load_cases[0].point[0].kind", "\"force\" or \"moment\""));
  EXPECT_TRUE(has(issues([](json& d){ d["load_cases"][0]["point"][0].erase("position"); }), "$.load_cases[0].point[0]", "missing required key 'position'"));
  EXPECT_TRUE(has(issues([](json& d){ d["load_cases"][0]["point"][0]["element"]=9; }), "$.load_cases[0].point[0].element", "unknown beam 9"));
  EXPECT_TRUE(has(issues([](json& d){ auto& ml=d["load_cases"][0]["member"][0]; ml["axes"]="global"; ml["projected"]=true; }),
                  "$.load_cases[0].member[0].projected", "\"kind\": \"force\""));
}

TEST(PointLoads, ForceEquilibriumMetricIsMeaningfulForMomentOnlyLoads) {
  // Regression: with only moments applied, applied and reaction forces are both
  // round-off, and the old normalization ||sum F|| / (||F_app|| + ||F_reac||)
  // returned ~1. The scale now also includes moments / model size.
  auto m=cantilever();
  LoadCase lc; lc.point={{7,LoadAxes::Local,2,M,40,0.4}};
  auto s=solve(m,lc);
  ASSERT_TRUE(s.status.success);
  EXPECT_LT(s.results.force_equilibrium_error, 1e-12);
  // The metric must still detect a real imbalance: perturb the free tip deflection.
  Eigen::VectorXd U=s.global;
  U(7)+=1e-3*std::abs(s.global(7));
  auto r=kopruq::LinearStaticSolver::recover(m,fixed1(),lc,U);
  EXPECT_GT(r.force_equilibrium_error, 1e-6);
}
