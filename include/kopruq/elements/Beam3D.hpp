#pragma once
#include "../core/Element.hpp"
#include <Eigen/Dense>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
namespace kopruq {
struct Beam3DGeometry { Eigen::Vector3d node1,node2,reference_y; };
// Asy/Asz: effective shear area [m^2], y- and z-bending planes; 0 (default) = rigid in shear (Euler-Bernoulli).
struct Beam3DProperties { Real E{},G{},A{},Iy{},Iz{},J{},Asy{0},Asz{0}; };
struct Beam3DAxes { Eigen::Vector3d x,y,z; };
using Beam3DMatrix=Eigen::Matrix<Real,12,12>;
enum class Beam3DErrorCode { NonFinite,InvalidLength,InvalidProperty,ZeroLength,ParallelReference,UnstableRelease };
struct Beam3DError:std::invalid_argument { Beam3DErrorCode code; Beam3DError(Beam3DErrorCode c,const std::string& m):std::invalid_argument(m),code(c){} };
class Beam3D final:public Element { public:
 explicit Beam3D(Id v):value_(v){} Id id()const override{return value_;}
 static Beam3DAxes axes(const Beam3DGeometry& g){ if(!g.node1.allFinite()||!g.node2.allFinite()||!g.reference_y.allFinite())throw Beam3DError(Beam3DErrorCode::NonFinite,"beam geometry contains a non-finite value");auto d=g.node2-g.node1;Real L=d.norm();if(!(L>0))throw Beam3DError(Beam3DErrorCode::ZeroLength,"beam element has zero length");Eigen::Vector3d x=d/L;Eigen::Vector3d y=g.reference_y-g.reference_y.dot(x)*x;if(!(y.norm()>64*std::numeric_limits<Real>::epsilon()*g.reference_y.norm()))throw Beam3DError(Beam3DErrorCode::ParallelReference,"reference orientation is parallel to the beam axis");y.normalize();Eigen::Vector3d z=x.cross(y);z.normalize();return{x,y,z};}
 static void validate(const Beam3DGeometry& g,const Beam3DProperties& p){axes(g);Real L=(g.node2-g.node1).norm();if(!std::isfinite(L)||L<=0)throw Beam3DError(Beam3DErrorCode::InvalidLength,"beam length must be finite and positive");if(!std::isfinite(p.E)||!std::isfinite(p.G)||!std::isfinite(p.A)||!std::isfinite(p.Iy)||!std::isfinite(p.Iz)||!std::isfinite(p.J)||!std::isfinite(p.Asy)||!std::isfinite(p.Asz))throw Beam3DError(Beam3DErrorCode::NonFinite,"beam property contains a non-finite value");if(p.E<=0||p.G<=0||p.A<=0||p.Iy<=0||p.Iz<=0||p.J<=0||p.Asy<0||p.Asz<0)throw Beam3DError(Beam3DErrorCode::InvalidProperty,"beam properties must be finite and positive (shear areas finite and non-negative)");}
 // Timoshenko shear correction: Phi = 12*E*I/(G*As*L^2), 0 when As == 0 (rigid in shear, i.e. Euler-Bernoulli;
 // every ay/by/cy/dy formula below reduces to the pre-Timoshenko values exactly when Phi == 0). Mass matrix
 // (MassMatrix.hpp) is unaffected -- it keeps the Euler-Bernoulli consistent shape functions; see its header.
 static Beam3DMatrix localStiffness(Real L,const Beam3DProperties&p){if(!std::isfinite(L)||L<=0)throw Beam3DError(Beam3DErrorCode::InvalidLength,"beam length must be finite and positive");if(!std::isfinite(p.E)||!std::isfinite(p.G)||!std::isfinite(p.A)||!std::isfinite(p.Iy)||!std::isfinite(p.Iz)||!std::isfinite(p.J)||!std::isfinite(p.Asy)||!std::isfinite(p.Asz)||p.E<=0||p.G<=0||p.A<=0||p.Iy<=0||p.Iz<=0||p.J<=0||p.Asy<0||p.Asz<0)throw Beam3DError(Beam3DErrorCode::InvalidProperty,"beam properties must be finite and positive (shear areas finite and non-negative)");Real EA=p.E*p.A/L,GJ=p.G*p.J/L;Real Phiy=p.Asy>0?12*p.E*p.Iz/(p.G*p.Asy*L*L):0,Phiz=p.Asz>0?12*p.E*p.Iy/(p.G*p.Asz*L*L):0;Real ay=12*p.E*p.Iz/(L*L*L*(1+Phiy)),by=6*p.E*p.Iz/(L*L*(1+Phiy)),cy=(4+Phiy)*p.E*p.Iz/(L*(1+Phiy)),dy=(2-Phiy)*p.E*p.Iz/(L*(1+Phiy)),az=12*p.E*p.Iy/(L*L*L*(1+Phiz)),bz=6*p.E*p.Iy/(L*L*(1+Phiz)),cz=(4+Phiz)*p.E*p.Iy/(L*(1+Phiz)),dz=(2-Phiz)*p.E*p.Iy/(L*(1+Phiz));Beam3DMatrix k=Beam3DMatrix::Zero();k(0,0)=k(6,6)=EA;k(0,6)=k(6,0)=-EA;k(3,3)=k(9,9)=GJ;k(3,9)=k(9,3)=-GJ;k(1,1)=k(7,7)=ay;k(1,7)=k(7,1)=-ay;k(1,5)=k(5,1)=by;k(1,11)=k(11,1)=by;k(7,5)=k(5,7)=-by;k(7,11)=k(11,7)=-by;k(5,5)=k(11,11)=cy;k(5,11)=k(11,5)=dy;k(2,2)=k(8,8)=az;k(2,8)=k(8,2)=-az;k(2,4)=k(4,2)=-bz;k(2,10)=k(10,2)=-bz;k(8,4)=k(4,8)=bz;k(8,10)=k(10,8)=bz;k(4,4)=k(10,10)=cz;k(4,10)=k(10,4)=dz;return k;}
 static Beam3DMatrix transformation(const Beam3DAxes&a){Beam3DMatrix T=Beam3DMatrix::Zero();Eigen::Matrix3d R;R.row(0)=a.x.transpose();R.row(1)=a.y.transpose();R.row(2)=a.z.transpose();T.block<3,3>(0,0)=R;T.block<3,3>(3,3)=R;T.block<3,3>(6,6)=R;T.block<3,3>(9,9)=R;return T;}
 static Beam3DMatrix globalStiffness(const Beam3DGeometry&g,const Beam3DProperties&p){validate(g,p);Real L=(g.node2-g.node1).norm();auto T=transformation(axes(g));return T.transpose()*localStiffness(L,p)*T;}
 private:Id value_;
};}
