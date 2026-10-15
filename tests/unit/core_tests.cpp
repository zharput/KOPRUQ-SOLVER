#include <gtest/gtest.h>
#include <Eigen/Dense>
#include "kopruq/solvers/LinearStaticSolver.hpp"

static kopruq::Model cantilever(unsigned long long id) {
  kopruq::Model m;
  m.nodes={{1,0,0,0},{2,4,0,0}};
  m.materials={{1,210000000,.3}};
  m.sections={{1,.3,.02,.04,.01}};
  m.beams={{id,1,2,1,1,{0,1,0}}};
  return m;
}
static std::map<kopruq::Id,std::array<bool,6>> fixed() {
  return {{1,{true,true,true,true,true,true}}};
}

TEST(ElementRecovery, IndependentAxialCantileverEndForces) {
  Eigen::Matrix<double,6,1> load; load<<100,0,0,0,0,0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(7),fixed(),{{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  ASSERT_EQ(s.results.elements.size(),1u);
  const double expected[12]={-100,0,0,0,0,0,100,0,0,0,0,0};
  const auto& e=s.results.elements.front();
  for(int i=0;i<12;++i){ASSERT_TRUE(std::isfinite(e.global_end_force[i]));EXPECT_NEAR(e.global_end_force[i],expected[i],1e-8);}
  EXPECT_EQ(e.element_id,7u); EXPECT_NEAR(e.force_balance.norm(),0,1e-10);
  EXPECT_NEAR(e.moment_balance.norm(),0,1e-10);
  EXPECT_LT(e.normalized_force_error,1e-12); EXPECT_LT(e.normalized_moment_error,1e-12);
  EXPECT_EQ(s.results.max_force_element_id,7u); EXPECT_EQ(s.results.max_moment_element_id,7u);
}

TEST(ElementRecovery, IndependentTransverseCantileverEndForces) {
  Eigen::Matrix<double,6,1> load; load<<0,-50,0,0,0,0;
  auto s=kopruq::LinearStaticSolver::solve(cantilever(8),fixed(),{{2,load}});
  ASSERT_TRUE(s.status.success)<<s.status.message;
  ASSERT_EQ(s.results.elements.size(),1u);
  const double expected[12]={0,50,0,0,0,200,0,-50,0,0,0,0};
  const auto& e=s.results.elements.front();
  for(int i=0;i<12;++i){ASSERT_TRUE(std::isfinite(e.global_end_force[i]));EXPECT_NEAR(e.global_end_force[i],expected[i],1e-8);}
  EXPECT_EQ(e.element_id,8u); EXPECT_NEAR(e.force_balance.norm(),0,1e-10);
  EXPECT_NEAR(e.moment_balance.norm(),0,1e-10);
  EXPECT_LT(e.normalized_force_error,1e-12); EXPECT_LT(e.normalized_moment_error,1e-12);
  EXPECT_EQ(s.results.max_force_element_id,8u); EXPECT_EQ(s.results.max_moment_element_id,8u);
}
