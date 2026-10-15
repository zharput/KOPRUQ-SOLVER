#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <cmath>
#include "kopruq/elements/Beam3D.hpp"

using kopruq::Beam3D;
using kopruq::Beam3DGeometry;
using kopruq::Beam3DProperties;
using Mat12 = Eigen::Matrix<double,12,12>;

static Mat12 independentLocal(double L, const Beam3DProperties& p) {
  Mat12 k=Mat12::Zero(); double ea=p.E*p.A/L, gj=p.G*p.J/L;
  double ay=12*p.E*p.Iz/(L*L*L), by=6*p.E*p.Iz/(L*L), cy=4*p.E*p.Iz/L, dy=2*p.E*p.Iz/L;
  double az=12*p.E*p.Iy/(L*L*L), bz=6*p.E*p.Iy/(L*L), cz=4*p.E*p.Iy/L, dz=2*p.E*p.Iy/L;
  k(0,0)=k(6,6)=ea;k(0,6)=k(6,0)=-ea;k(3,3)=k(9,9)=gj;k(3,9)=k(9,3)=-gj;
  k(1,1)=k(7,7)=ay;k(1,7)=k(7,1)=-ay;k(1,5)=k(5,1)=by;k(1,11)=k(11,1)=by;k(7,5)=k(5,7)=-by;k(7,11)=k(11,7)=-by;k(5,5)=k(11,11)=cy;k(5,11)=k(11,5)=dy;
  k(2,2)=k(8,8)=az;k(2,8)=k(8,2)=-az;k(2,4)=k(4,2)=-bz;k(2,10)=k(10,2)=-bz;k(8,4)=k(4,8)=bz;k(8,10)=k(10,8)=bz;k(4,4)=k(10,10)=cz;k(4,10)=k(10,4)=dz;
  return k;
}
static Eigen::Matrix3d independentR(const Beam3DGeometry& g) {
  Eigen::Vector3d x=(g.node2-g.node1).normalized(); Eigen::Vector3d y=g.reference_y-g.reference_y.dot(x)*x; y.normalize(); Eigen::Vector3d z=x.cross(y).normalized(); Eigen::Matrix3d R; R.row(0)=x.transpose();R.row(1)=y.transpose();R.row(2)=z.transpose();return R;
}

TEST(Beam3DRequalification, IndependentLocalCoefficientsAndSigns) {
  Beam3DProperties p{210e6,80e6,.7,.031,.097,.013}; double L=4.3;
  Mat12 expected=independentLocal(L,p), actual=Beam3D::localStiffness(L,p);
  EXPECT_LT((actual-expected).cwiseAbs().maxCoeff(),1e-12*expected.cwiseAbs().maxCoeff());
  EXPECT_DOUBLE_EQ(actual(0,0),p.E*p.A/L); EXPECT_DOUBLE_EQ(actual(3,3),p.G*p.J/L);
  EXPECT_DOUBLE_EQ(actual(1,5),6*p.E*p.Iz/(L*L)); EXPECT_DOUBLE_EQ(actual(2,4),-6*p.E*p.Iy/(L*L));
  EXPECT_DOUBLE_EQ(actual(5,11),2*p.E*p.Iz/L); EXPECT_DOUBLE_EQ(actual(4,10),2*p.E*p.Iy/L);
  EXPECT_TRUE(actual.allFinite()); EXPECT_TRUE(actual.isApprox(actual.transpose(),1e-12));
}

TEST(Beam3DRequalification, RankAndSixRigidModes) {
  Beam3DProperties p{210e6,80e6,.7,.031,.097,.013}; Mat12 k=Beam3D::localStiffness(4.3,p);
  Eigen::SelfAdjointEigenSolver<Mat12> es((k+k.transpose())*.5); ASSERT_EQ(es.info(),Eigen::Success);
  double scale=es.eigenvalues().cwiseAbs().maxCoeff(); int zero=0,nonzero=0; for(int i=0;i<12;++i){if(std::abs(es.eigenvalues()(i))<1e-10*scale)++zero;else if(es.eigenvalues()(i)>0)++nonzero;}
  EXPECT_EQ(zero,6); EXPECT_EQ(nonzero,6); EXPECT_TRUE(k.allFinite());
}

TEST(Beam3DRequalification, IndependentAxesAndTransformation) {
  Beam3DProperties p{210e6,80e6,.7,.031,.097,.013}; Beam3DGeometry g{{1,2,3},{5,7,11},{0,1,1}};
  auto R=independentR(g); EXPECT_NEAR(R.determinant(),1,1e-12); EXPECT_TRUE((R*R.transpose()).isApprox(Eigen::Matrix3d::Identity(),1e-12));
  auto a=Beam3D::axes(g); EXPECT_NEAR(a.x.dot(a.y),0,1e-12); EXPECT_NEAR(a.x.cross(a.y).dot(a.z),1,1e-12);
  Mat12 T=Mat12::Zero(); for(int i=0;i<4;++i)T.block<3,3>(3*i,3*i)=R;
  Mat12 expected=T.transpose()*independentLocal((g.node2-g.node1).norm(),p)*T; Mat12 actual=Beam3D::globalStiffness(g,p);
  EXPECT_LT((actual-expected).norm()/expected.norm(),1e-12); EXPECT_TRUE(actual.isApprox(actual.transpose(),1e-12));
  Eigen::Matrix<double,12,1> u=Eigen::Matrix<double,12,1>::Random(); EXPECT_NEAR((u.transpose()*actual*u)(0),(T*u).transpose()*independentLocal((g.node2-g.node1).norm(),p)*(T*u),1e-8*std::abs((u.transpose()*actual*u)(0))+1e-8);
}

TEST(Beam3DRequalification, HorizontalVerticalInclinedAndReversedAxes) {
  Beam3DProperties p{210e6,80e6,.7,.031,.097,.013};
  for(const auto& g: {Beam3DGeometry{{0,0,0},{4,0,0},{0,1,0}},Beam3DGeometry{{0,0,0},{0,0,4},{1,0,0}},Beam3DGeometry{{0,0,0},{2,3,4},{0,0,1}}}) { auto a=Beam3D::axes(g); EXPECT_NEAR(a.x.norm(),1,1e-12); EXPECT_NEAR(a.y.norm(),1,1e-12); EXPECT_NEAR(a.z.norm(),1,1e-12); EXPECT_NEAR(a.x.cross(a.y).dot(a.z),1,1e-12); EXPECT_TRUE(Beam3D::globalStiffness(g,p).allFinite()); }
  Beam3DGeometry rev{{4,0,0},{0,0,0},{0,1,0}}; auto ar=Beam3D::axes(rev); EXPECT_NEAR(ar.x.dot(Eigen::Vector3d(-1,0,0)),1,1e-12);
}

TEST(Beam3DRequalification, InvalidGeometryAndProperties) {
  Beam3DProperties good{210e6,80e6,.7,.031,.097,.013};
  EXPECT_THROW(Beam3D::axes({{0,0,0},{0,0,0},{0,1,0}}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::axes({{0,0,0},{1,0,0},{1,0,0}}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::localStiffness(4,{-1,good.G,good.A,good.Iy,good.Iz,good.J}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::localStiffness(4,{good.E,0,good.A,good.Iy,good.Iz,good.J}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::localStiffness(4,{good.E,good.G,0,good.Iy,good.Iz,good.J}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::localStiffness(4,{good.E,good.G,good.A,0,good.Iz,good.J}),kopruq::Beam3DError);
  EXPECT_THROW(Beam3D::localStiffness(4,{good.E,good.G,good.A,good.Iy,good.Iz,0}),kopruq::Beam3DError);
}
