#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include "kopruq/assembly/MatrixAssembler.hpp"
#include "kopruq/assembly/DofManager.hpp"

using D=double; using M=Eigen::MatrixXd;
static M localRef(D L,D E,D G,D A,D Iy,D Iz,D J){M k=M::Zero(12,12);D ea=E*A/L,gj=G*J/L,ay=12*E*Iz/(L*L*L),by=6*E*Iz/(L*L),cy=4*E*Iz/L,dy=2*E*Iz/L,az=12*E*Iy/(L*L*L),bz=6*E*Iy/(L*L),cz=4*E*Iy/L,dz=2*E*Iy/L;k(0,0)=k(6,6)=ea;k(0,6)=k(6,0)=-ea;k(3,3)=k(9,9)=gj;k(3,9)=k(9,3)=-gj;k(1,1)=k(7,7)=ay;k(1,7)=k(7,1)=-ay;k(1,5)=k(5,1)=by;k(1,11)=k(11,1)=by;k(7,5)=k(5,7)=-by;k(7,11)=k(11,7)=-by;k(5,5)=k(11,11)=cy;k(5,11)=k(11,5)=dy;k(2,2)=k(8,8)=az;k(2,8)=k(8,2)=-az;k(2,4)=k(4,2)=-bz;k(2,10)=k(10,2)=-bz;k(8,4)=k(4,8)=bz;k(8,10)=k(10,8)=bz;k(4,4)=k(10,10)=cz;k(4,10)=k(10,4)=dz;return k;}
static Eigen::Matrix3d basis(const Eigen::Vector3d&a,const Eigen::Vector3d&b,const Eigen::Vector3d&ref){auto x=(b-a).normalized();auto y=(ref-ref.dot(x)*x).normalized();auto z=x.cross(y).normalized();Eigen::Matrix3d R;R.row(0)=x.transpose();R.row(1)=y.transpose();R.row(2)=z.transpose();return R;}
static M ke(const Eigen::Vector3d&a,const Eigen::Vector3d&b,const Eigen::Vector3d&ref,D A,D Iy,D Iz,D J){auto R=basis(a,b,ref);M T=M::Zero(12,12);for(int i=0;i<4;++i)T.block<3,3>(3*i,3*i)=R;return T.transpose()*localRef((b-a).norm(),210e6,210e6/(2*(1+.3)),A,Iy,Iz,J)*T;}
static kopruq::Model one(){kopruq::Model m;m.nodes={{10,0,0,0},{30,4,0,0}};m.materials={{1,210e6,.3}};m.sections={{1,.3,.02,.04,.01}};m.beams={{1,10,30,1,1,{0,1,0}}};return m;}

TEST(AssemblyRequalification, IndependentDofMapAndOrdering){kopruq::Model m=one();kopruq::DofManager d(m);EXPECT_EQ(d.dof_count(),12u);auto a=d.node_dofs(10),b=d.node_dofs(30);for(int i=0;i<6;++i){EXPECT_EQ(a[i],(size_t)i);EXPECT_EQ(b[i],(size_t)(6+i));}EXPECT_EQ((int)kopruq::Dof::UX,0);EXPECT_EQ((int)kopruq::Dof::RZ,5);kopruq::Model rev=m;std::reverse(rev.nodes.begin(),rev.nodes.end());kopruq::DofManager dr(rev);EXPECT_EQ(dr.node_dofs(10),a);kopruq::Model bad;bad.nodes={{1,0,0,0},{1,1,0,0}};EXPECT_THROW((kopruq::DofManager{bad}),kopruq::AssemblyError);}
TEST(AssemblyRequalification, IndependentSingleElementMatrix){auto m=one();auto K=kopruq::MatrixAssembler::assemble(m);M ref=ke({0,0,0},{4,0,0},{0,1,0},.3,.02,.04,.01);ASSERT_EQ(K.rows(),12);M dense(K);EXPECT_LT((dense-ref).cwiseAbs().maxCoeff(),1e-9*ref.cwiseAbs().maxCoeff());EXPECT_TRUE(dense.isApprox(dense.transpose(),1e-12));EXPECT_TRUE(dense.allFinite());}
TEST(AssemblyRequalification, IndependentTwoElementSharedNode18x18){kopruq::Model m;m.nodes={{50,0,0,0},{10,3,4,0},{90,6,4,5}};m.materials={{1,210e6,.3}};m.sections={{1,.3,.02,.04,.01},{2,.5,.03,.07,.02}};m.beams={{4,50,10,1,1,{0,0,1}},{9,10,90,1,2,{0,1,0}}};auto K=kopruq::MatrixAssembler::assemble(m);M ref=M::Zero(18,18);M k1=ke({0,0,0},{3,4,0},{0,0,1},.3,.02,.04,.01),k2=ke({3,4,0},{6,4,5},{0,1,0},.5,.03,.07,.02);int map1[12]={6,7,8,9,10,11,0,1,2,3,4,5},map2[12]={0,1,2,3,4,5,12,13,14,15,16,17};for(int i=0;i<12;++i)for(int j=0;j<12;++j){ref(map1[i],map1[j])+=k1(i,j);ref(map2[i],map2[j])+=k2(i,j);}M dense(K);double err=(dense-ref).norm()/ref.norm();double shared=dense.block(0,0,6,6).norm();double k1shared=k1.block(6,6,6,6).norm();EXPECT_EQ(K.rows(),18);EXPECT_LT(err,1e-11);EXPECT_TRUE(dense.isApprox(dense.transpose(),1e-12));EXPECT_GT(shared,k1shared);}
TEST(AssemblyRequalification, RestraintsComponentsAndOrientation){kopruq::Model m=one();kopruq::DofManager d(m);auto b=kopruq::MatrixAssembler::classify(d,std::map<kopruq::Id,std::array<bool,6>>{{10,{true,true,false,false,true,false}},{30,{false,false,false,false,false,false}}});EXPECT_EQ(b.fixed.size(),3u);EXPECT_EQ(b.free.size(),9u);auto c=kopruq::MatrixAssembler::components(m);ASSERT_EQ(c.components.size(),1u);EXPECT_TRUE(c.isolated.empty());auto v=m;v.nodes[1]={30,0,0,4};v.nodes[0]={10,0,0,0};v.beams[0].reference_y={1,0,0};auto K=kopruq::MatrixAssembler::assemble(v);M dense(K);EXPECT_TRUE(dense.allFinite());}
TEST(AssemblyRequalification, InvalidAssemblyInputs){
  auto m=one(); m.beams[0].node2=999;
  EXPECT_THROW(kopruq::MatrixAssembler::assemble(m),kopruq::AssemblyError);
  kopruq::Model empty;
  EXPECT_THROW(([&]{kopruq::DofManager x(empty);}()),kopruq::AssemblyError);
  m=one(); m.beams[0].id=2; m.beams.push_back(m.beams[0]);
  EXPECT_THROW(kopruq::MatrixAssembler::assemble(m),kopruq::AssemblyError);
}
