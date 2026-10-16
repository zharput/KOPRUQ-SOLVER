// Phase 5.1 — beam end releases (static condensation).
//
// Expected values are closed-form results for structures whose hinges are
// modelled with end releases: simply supported and propped beams, a Gerber
// beam with an internal hinge (statics + superposition), and a portal frame
// with a pinned beam under lateral load (load sharing from the column and
// beam stiffnesses). None use the production condensation.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include "kopruq/io/ModelJson.hpp"

using kopruq::LoadAxes;
using kopruq::LoadCase;
using kopruq::MemberLoad;
using Vec6 = Eigen::Matrix<double,6,1>;
using RestraintMap = std::map<kopruq::Id,std::array<bool,6>>;

namespace {

constexpr double E=210e6, NU=.3, A=.3, IY=.02, IZ=.04, J=.01;
constexpr std::array<bool,6> FIXED{true,true,true,true,true,true};

kopruq::Releases rel(std::initializer_list<int> dofs) { kopruq::Releases r{}; for (int d : dofs) r[d]=true; return r; }

kopruq::Model line(const std::vector<double>& xs) {
  kopruq::Model m;
  for (size_t i=0;i<xs.size();++i) m.nodes.push_back({kopruq::Id(i+1),xs[i],0,0});
  m.materials={{1,E,NU}}; m.sections={{1,A,IY,IZ,J}};
  for (size_t i=0;i+1<xs.size();++i) m.beams.push_back({kopruq::Id(i+1),kopruq::Id(i+1),kopruq::Id(i+2),1,1,{0,1,0}});
  return m;
}
const kopruq::NodeResults& nodeOf(const kopruq::StaticSolution& s, kopruq::Id id) {
  for (const auto& n : s.results.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
const kopruq::ElementResults& elementOf(const kopruq::StaticSolution& s, kopruq::Id id) {
  for (const auto& e : s.results.elements) if (e.element_id == id) return e;
  throw std::runtime_error("element not found");
}
kopruq::StaticSolution solve(const kopruq::Model& m, const RestraintMap& rs, const LoadCase& lc) {
  return kopruq::LinearStaticSolver::solveCases(m,rs,{lc}).cases.front();
}
void rel9(double actual, double expected, const char* what) {
  EXPECT_NEAR(actual, expected, 1e-9*std::max(std::abs(expected),1e-300)) << what;
}
void clean(const kopruq::StaticSolution& s) {
  ASSERT_TRUE(s.status.success) << s.status.message;
  EXPECT_LT(s.relative_residual, 1e-14);
  EXPECT_LT(s.results.force_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.moment_equilibrium_error, 1e-12);
  EXPECT_LT(s.results.max_element_moment_error, 1e-12);
  EXPECT_LT(s.results.energy_error, 1e-10);
}
LoadCase uniformY(std::initializer_list<kopruq::Id> elems, double q) {
  LoadCase lc; for (auto e : elems) lc.member.push_back(MemberLoad{e,LoadAxes::Local,1,q,q,0,1}); return lc;
}

} // namespace

TEST(Releases, SimplySupportedBeamFromMomentReleases) {
  double L=8, q=-10, EI=E*IZ;
  auto m=line({0,L/2,L});
  m.beams[0].releases=rel({5});        // Mz released at node 1
  m.beams[1].releases=rel({11});       // Mz released at node 3
  auto s=solve(m,{{1,FIXED},{3,FIXED}},uniformY({1,2},q));
  clean(s);
  rel9(nodeOf(s,2).displacement[1], 5*q*std::pow(L,4)/(384*EI), "v_mid = 5qL^4/384EI");
  rel9(nodeOf(s,1).reaction[1], -q*L/2, "R_A");
  rel9(nodeOf(s,3).reaction[1], -q*L/2, "R_B");
  EXPECT_NEAR(nodeOf(s,1).reaction[5], 0.0, 1e-9*std::abs(q*L*L)) << "no moment at a released end";
  EXPECT_NEAR(nodeOf(s,3).reaction[5], 0.0, 1e-9*std::abs(q*L*L));
  EXPECT_NEAR(elementOf(s,1).local_end_force[5], 0.0, 1e-9*std::abs(q*L*L));
  rel9(elementOf(s,1).local_end_force[11], -q*L*L/8, "midspan moment qL^2/8");
}

TEST(Releases, ProppedCantileverFromOneMomentRelease) {
  double L=6, q=-12, EI=E*IZ;
  auto m=line({0,L/2,L});
  m.beams[1].releases=rel({11});
  auto s=solve(m,{{1,FIXED},{3,FIXED}},uniformY({1,2},q));
  clean(s);
  rel9(nodeOf(s,2).displacement[1], q*std::pow(L,4)/(192*EI), "v_mid = qL^4/192EI");
  rel9(nodeOf(s,1).reaction[1], -5*q*L/8, "R_A = 5qL/8");
  rel9(nodeOf(s,3).reaction[1], -3*q*L/8, "R_B = 3qL/8");
  rel9(nodeOf(s,1).reaction[5], -q*L*L/8, "M_A = qL^2/8");
  EXPECT_NEAR(nodeOf(s,3).reaction[5], 0.0, 1e-9*std::abs(q*L*L));
}

TEST(Releases, GerberBeamWithInternalHinge) {
  // A fixed (x=0), hinge at B (x=a), load P at D (midpoint of BC), pin at C (x=a+b).
  double a=3, b=4, P=-20, EI=E*IZ;
  auto m=line({0,a,a+b/2,a+b});
  m.beams[1].releases=rel({5});        // element B-D released for Mz at B
  RestraintMap rs={{1,FIXED},{4,{true,true,true,true,true,false}}};
  LoadCase lc; lc.nodal={{3,(Vec6()<<0,P,0,0,0,0).finished()}};
  auto s=solve(m,rs,lc);
  clean(s);
  double vB=(P/2)*a*a*a/(3*EI);
  rel9(nodeOf(s,2).displacement[1], vB, "v_B: cantilever under the hinge shear P/2");
  rel9(nodeOf(s,3).displacement[1], vB/2+P*b*b*b/(48*EI), "v_D = v_B/2 + Pb^3/48EI");
  rel9(nodeOf(s,4).reaction[1], -P/2, "R_C");
  rel9(nodeOf(s,1).reaction[1], -P/2, "R_A");
  rel9(nodeOf(s,1).reaction[5], -a*P/2, "M_A");
  EXPECT_NEAR(elementOf(s,2).local_end_force[5], 0.0, 1e-9*std::abs(P*a)) << "no moment through the hinge";
}

TEST(Releases, PortalFrameWithPinnedBeamSharesLateralLoad) {
  double h=4, Lb=6, H=30;
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,0,h,0},{3,Lb,h,0},{4,Lb,0,0}};
  m.materials={{1,E,NU}}; m.sections={{1,A,IY,IZ,J}};
  m.beams={{1,1,2,1,1,{1,0,0}},{2,2,3,1,1,{0,1,0}},{3,4,3,1,1,{1,0,0}}};
  m.beams[1].releases=rel({5,11});     // beam pinned to both columns (in-plane)
  LoadCase lc; lc.nodal={{2,(Vec6()<<H,0,0,0,0,0).finished()}};
  auto s=solve(m,{{1,FIXED},{4,FIXED}},lc);
  clean(s);
  // Columns: cantilevers with free top rotation, k_c = 3EI/h^3 (in-plane bending uses Iz);
  // beam: axial link, k_b = EA/Lb. N = H k_b / (k_c + 2 k_b).
  double kc=3*E*IZ/(h*h*h), kb=E*A/Lb, N=H*kb/(kc+2*kb);
  rel9(nodeOf(s,2).displacement[0], (H-N)/kc, "u_2");
  rel9(nodeOf(s,3).displacement[0], N/kc, "u_3");
  rel9(nodeOf(s,1).reaction[0], -(H-N), "column 1 shear");
  rel9(nodeOf(s,4).reaction[0], -N, "column 2 shear");
  rel9(nodeOf(s,1).reaction[5], h*(H-N), "column 1 base moment");
  rel9(nodeOf(s,4).reaction[5], h*N, "column 2 base moment");
  EXPECT_NEAR(nodeOf(s,1).reaction[1], 0.0, 1e-9*H) << "pinned beam carries no shear";
  const auto& beam=elementOf(s,2);
  for (int i : {1,5,7,11}) EXPECT_NEAR(beam.local_end_force[i], 0.0, 1e-9*H*h) << "beam shear/moment " << i;
  rel9(std::abs(beam.local_end_force[0]), N, "beam axial force");
}

TEST(Releases, UnstableReleaseCombinationsAreRejected) {
  for (auto r : {rel({3,9}), rel({0,6}), rel({1,7}), rel({1,5,7,11})}) {
    auto m=line({0,4});
    m.beams[0].releases=r;
    auto s=solve(m,{{1,FIXED},{2,FIXED}},uniformY({1},-1));
    EXPECT_FALSE(s.status.success);
    EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::InvalidModel);
    EXPECT_NE(s.status.message.find("unstable"), std::string::npos) << s.status.message;
  }
}

TEST(Releases, ReleasedDofWithNoOtherStiffnessIsReportedSingular) {
  auto m=line({0,4});
  m.beams[0].releases=rel({11});       // node 2 has no rotational stiffness about z
  auto s=solve(m,{{1,FIXED}},uniformY({1},-1));
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::SingularSystem);
}

TEST(Releases, JsonReadsWritesAndValidatesReleases) {
  using kopruq::io::json;
  json doc=json::parse(R"({
    "schema": "kopruq-model", "schema_version": 1,
    "nodes": [{"id": 1, "x": 0, "y": 0, "z": 0}, {"id": 2, "x": 3, "y": 0, "z": 0}, {"id": 3, "x": 6, "y": 0, "z": 0}],
    "materials": [{"id": 1, "elastic_modulus": 210e6, "poisson_ratio": 0.3}],
    "sections": [{"id": 1, "area": 0.3, "Iy": 0.02, "Iz": 0.04, "J": 0.01}],
    "beams": [{"id": 1, "nodes": [1, 2], "material": 1, "section": 1},
              {"id": 2, "nodes": [2, 3], "material": 1, "section": 1,
               "releases": {"end": [false, false, false, false, false, true]}}],
    "supports": [{"node": 1, "restraints": [true, true, true, true, true, true]},
                 {"node": 3, "restraints": [true, true, true, true, true, true]}],
    "load_cases": [{"name": "Q", "member": [{"element": 1, "axes": "local", "direction": "y", "w1": -12},
                                            {"element": 2, "axes": "local", "direction": "y", "w1": -12}]}]
  })");
  auto in=kopruq::io::readModel(doc);
  EXPECT_EQ(in.model.beams[1].releases, rel({11}));
  EXPECT_EQ(in.model.beams[0].releases, kopruq::Releases{});
  EXPECT_EQ(kopruq::io::writeModel(kopruq::io::readModelText(kopruq::io::writeModel(in).dump())), kopruq::io::writeModel(in));
  auto r=kopruq::LinearStaticSolver::solveCases(in.model,in.restraints,in.load_cases).cases[0];
  ASSERT_TRUE(r.status.success);
  rel9(nodeOf(r,1).reaction[1], 12*6*5/8.0, "propped cantilever R_A = 5qL/8 via JSON");
  auto issues=[&](auto mutate){ json d=doc; mutate(d); try{ kopruq::io::readModel(d); }catch(const kopruq::io::ModelInputError& e){ return e.issues; } return std::vector<kopruq::io::InputIssue>{}; };
  auto has=[](const std::vector<kopruq::io::InputIssue>& v,const std::string& path,const std::string& text){
    return std::any_of(v.begin(),v.end(),[&](const auto& i){ return i.path==path && i.message.find(text)!=std::string::npos; }); };
  EXPECT_TRUE(has(issues([](json& d){ d["beams"][1]["releases"]["start"]=json::array({true,false,false,true,false}); }), "$.beams[1].releases.start", "6 booleans"));
  EXPECT_TRUE(has(issues([](json& d){ d["beams"][1]["releases"]["middle"]=json::array(); }), "$.beams[1].releases.middle", "unknown key"));
  EXPECT_TRUE(has(issues([](json& d){ d["beams"][1]["releases"]["start"]=json::array({false,false,false,true,false,false});
                                       d["beams"][1]["releases"]["end"]=json::array({false,false,false,true,false,false}); }),
                  "$.beams[1].releases", "unstable"));
}
