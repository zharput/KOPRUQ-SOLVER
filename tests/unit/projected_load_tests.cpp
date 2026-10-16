// Phase 4.1b — global member loads per projected length.
//
// A projected load w acts per unit length of the member's projection onto
// the plane normal to the load direction. Expected values: the projected
// length is computed here from node coordinates, reactions from plain vector
// statics (resultant w * L_projected at the member midpoint), and
// displacements from the true-length path (verified independently in
// member_load_tests.cpp) with the intensity w * L_projected / L.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/solvers/LinearStaticSolver.hpp"

using kopruq::LoadAxes;
using kopruq::LoadCase;
using kopruq::MemberLoad;

namespace {

kopruq::Model member(const Eigen::Vector3d& b, const Eigen::Vector3d& ref) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,b.x(),b.y(),b.z()}};
  m.materials={{1,210e6,.3}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{41,1,2,1,1,ref}};
  return m;
}
std::map<kopruq::Id,std::array<bool,6>> fixedNode1() { return {{1,{true,true,true,true,true,true}}}; }
kopruq::StaticSolution solveWith(const kopruq::Model& m, const MemberLoad& l) {
  LoadCase lc; lc.member={l};
  return kopruq::LinearStaticSolver::solveCases(m,fixedNode1(),{lc}).cases.front();
}
const kopruq::NodeResults& nodeOf(const kopruq::AnalysisResults& r, kopruq::Id id) {
  for (const auto& n : r.nodes) if (n.node_id == id) return n;
  throw std::runtime_error("node not found");
}
// Length of segment 0->b projected onto the plane normal to global axis `dir`.
double projectedLength(Eigen::Vector3d b, int dir) { b(dir)=0; return b.norm(); }

void expectStatics(const kopruq::StaticSolution& s, const Eigen::Vector3d& b, int dir, double resultant) {
  Eigen::Vector3d F=Eigen::Vector3d::Zero(); F(dir)=resultant;
  Eigen::Vector3d M=(0.5*b).cross(F);
  const auto& root=nodeOf(s.results,1);
  double fs=std::max(std::abs(resultant),1e-300), ms=std::max(M.norm(),1e-300);
  for (int i=0;i<3;++i) EXPECT_NEAR(root.reaction[i], -F(i), 1e-9*fs) << "force " << i;
  for (int i=0;i<3;++i) EXPECT_NEAR(root.reaction[3+i], -M(i), 1e-9*ms) << "moment " << i;
}

} // namespace

TEST(ProjectedLoads, GravityOnInclinedMemberUsesHorizontalProjection) {
  Eigen::Vector3d b(3,0,4);      // L = 5, horizontal projection = 3
  double w=-6.0, Lp=projectedLength(b,2);
  ASSERT_DOUBLE_EQ(Lp,3.0);
  auto m=member(b,{0,1,0});
  auto p=solveWith(m,MemberLoad{41,LoadAxes::Global,2,w,w,0,1,true});
  ASSERT_TRUE(p.status.success)<<p.status.message;
  expectStatics(p,b,2,w*Lp);
  auto t=solveWith(m,MemberLoad{41,LoadAxes::Global,2,w*Lp/b.norm(),w*Lp/b.norm(),0,1,false});
  ASSERT_TRUE(t.status.success);
  EXPECT_LT((p.global-t.global).cwiseAbs().maxCoeff(), 1e-12*t.global.cwiseAbs().maxCoeff());
  // And it differs from the default true-length interpretation by L_p/L.
  auto d=solveWith(m,MemberLoad{41,LoadAxes::Global,2,w,w,0,1});
  expectStatics(d,b,2,w*b.norm());
}

TEST(ProjectedLoads, HorizontalLoadOnInclinedMemberUsesProjectionNormalToX) {
  Eigen::Vector3d b(3,0,4);      // plane normal to X keeps the Z extent: 4
  double w=5.0, Lp=projectedLength(b,0);
  ASSERT_DOUBLE_EQ(Lp,4.0);
  auto s=solveWith(member(b,{0,1,0}),MemberLoad{41,LoadAxes::Global,0,w,w,0,1,true});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  expectStatics(s,b,0,w*Lp);
}

TEST(ProjectedLoads, PartialTrapezoidScalesWithProjection) {
  Eigen::Vector3d b(2,2,1);      // L = 3, horizontal projection = sqrt(8)
  double w1=-2.0, w2=-8.0, a=0.2, e=0.7, Lp=projectedLength(b,2);
  auto s=solveWith(member(b,{0,0,1}),MemberLoad{41,LoadAxes::Global,2,w1,w2,a,e,true});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  // Resultant: mean intensity times the projected loaded length.
  double resultant=0.5*(w1+w2)*(e-a)*Lp;
  double centroid=a+(e-a)*(w1+2*w2)/(3*(w1+w2));     // fraction of member length
  Eigen::Vector3d F(0,0,resultant), M=(centroid*b).cross(F);
  const auto& root=nodeOf(s.results,1);
  EXPECT_NEAR(root.reaction[2], -resultant, 1e-9*std::abs(resultant));
  for (int i=0;i<3;++i) EXPECT_NEAR(root.reaction[3+i], -M(i), 1e-9*M.norm()) << "moment " << i;
}

TEST(ProjectedLoads, HorizontalMemberIsUnaffectedAndAxialMemberGetsNoGravity) {
  // Horizontal member: projection normal to Z equals the true length.
  Eigen::Vector3d h(4,0,0);
  auto mh=member(h,{0,1,0});
  auto p=solveWith(mh,MemberLoad{41,LoadAxes::Global,2,-7,-7,0,1,true});
  auto t=solveWith(mh,MemberLoad{41,LoadAxes::Global,2,-7,-7,0,1,false});
  ASSERT_TRUE(p.status.success && t.status.success);
  EXPECT_EQ((p.global-t.global).cwiseAbs().maxCoeff(), 0.0);
  // Vertical member under projected gravity: its projection is a point, so no load.
  Eigen::Vector3d v(0,0,4);
  auto pv=solveWith(member(v,{1,0,0}),MemberLoad{41,LoadAxes::Global,2,-7,-7,0,1,true});
  ASSERT_TRUE(pv.status.success)<<pv.status.message;
  EXPECT_EQ(pv.global.cwiseAbs().maxCoeff(), 0.0);
  // Same vertical member, projected horizontal load: projection normal to X is the full length.
  auto px=solveWith(member(v,{1,0,0}),MemberLoad{41,LoadAxes::Global,0,3,3,0,1,true});
  ASSERT_TRUE(px.status.success);
  expectStatics(px,v,0,3*4.0);
}

TEST(ProjectedLoads, ProjectedLocalLoadIsRejected) {
  auto s=solveWith(member({4,0,0},{0,1,0}),MemberLoad{41,LoadAxes::Local,1,-1,-1,0,1,true});
  EXPECT_FALSE(s.status.success);
  EXPECT_EQ(s.status.code, kopruq::SolverStatusCode::InvalidModel);
}
