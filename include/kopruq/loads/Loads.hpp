#pragma once
// Load definitions and consistent equivalent nodal loads for Beam3D members.
//
// Conventions (approved for Phase 4.1, 2026-10-16):
// - Member loads are distributed forces in kN/m, uniform or linearly varying
//   over a segment [start, end] given as fractions of the member length
//   (0 = node1, 1 = node2), in member-local or global axes.
// - Global-axis intensities act per unit of true member length by default.
//   With projected = true they act per unit length of the member's
//   projection onto the plane normal to the load direction (for gravity
//   loads: the horizontal projection), i.e. the true-length intensity is
//   w * L_projected / L. Only valid for global-axis loads.
// - Equivalent nodal loads are the consistent Euler-Bernoulli load vector
//   f = integral(N^T q dx) with the element's cubic Hermite / linear shape
//   functions, which equals the negated exact fixed-end forces. Element end
//   forces are recovered as k*u - f.
// - Concentrated forces/moments on a member (MemberPointLoad) and distributed
//   moments (MemberLoad with kind = Moment, kN*m/m) use the same consistent
//   load vector: a moment does work through the rotation it is conjugate to,
//   so it enters through the shape-function derivatives (theta_z = dv/dx,
//   theta_y = -dw/dx) and, for torsion, the linear shape functions.
// - Self-weight: unit_weight [kN/m^3] * area [m^2] per true length, acting in
//   global -Z, scaled by LoadCase::self_weight.
#include "../core/Types.hpp"
#include <Eigen/Dense>
#include <string>
#include <utility>
#include <vector>

namespace kopruq {

struct NodalLoad { Id node{}; Eigen::Matrix<Real,6,1> values=Eigen::Matrix<Real,6,1>::Zero(); };

enum class LoadAxes { Local, Global };
enum class MemberLoadKind { Force, Moment };

struct MemberLoad {
  Id element{};
  LoadAxes axes{LoadAxes::Local};
  int direction{};          // 0, 1, 2 = x, y, z of the chosen axes
  Real w1{}, w2{};          // intensity at start and end of the loaded segment [kN/m]
  Real start{0}, end{1};    // loaded segment as fractions of member length
  bool projected{false};    // global force loads only: intensity per projected length (see above)
  MemberLoadKind kind{MemberLoadKind::Force};   // Moment: distributed moment about `direction` [kN*m/m]
};

// Concentrated force [kN] or moment [kN*m] at `position` (fraction of member length from node1).
struct MemberPointLoad {
  Id element{};
  LoadAxes axes{LoadAxes::Local};
  int direction{};          // 0, 1, 2 = x, y, z of the chosen axes (moment: axis of rotation)
  MemberLoadKind kind{MemberLoadKind::Force};
  Real value{};
  Real position{};
};

// Uniform and/or linear-gradient temperature change on a member (Phase 7T, kopruq/loads/
// ThermalLoads.hpp has the equivalent-nodal-load derivation). gradient_*_delta_t != 0 requires
// the matching gradient_*_depth > 0 (the section depth the gradient acts over).
struct MemberThermalLoad {
  Id element{};
  Real uniform_delta_t{0};
  Real gradient_y_delta_t{0}, gradient_y_depth{0};
  Real gradient_z_delta_t{0}, gradient_z_depth{0};
};

struct LoadCase {
  std::string name;
  std::vector<NodalLoad> nodal;
  std::vector<MemberLoad> member;
  Real self_weight{};       // factor on self-weight; 0 = no self-weight
  std::vector<MemberPointLoad> point;
  std::vector<MemberThermalLoad> thermal;
};

struct LoadCombination {
  std::string name;
  std::vector<std::pair<std::size_t,Real>> terms;   // (load case index, factor)
};

namespace detail {

using Vec12 = Eigen::Matrix<Real,12,1>;

// Effect of one member load, in member-local axes.
struct MemberLoadEffect {
  Vec12 equivalent=Vec12::Zero();                         // consistent equivalent nodal loads
  Eigen::Vector3d force=Eigen::Vector3d::Zero();          // resultant force
  Eigen::Vector3d moment_node1=Eigen::Vector3d::Zero();   // resultant moment about node1
};

// Adds weight * (force or moment vector v, local axes) applied at distance x from node1.
inline void addLocalAction(MemberLoadEffect& e,Real x,Real L,const Eigen::Vector3d& v,MemberLoadKind kind,Real w){
  const Real s=x/L, s2=s*s, s3=s2*s;
  const Real N1=1-s, N2=s;
  if(kind==MemberLoadKind::Force){
    const Real H1=1-3*s2+2*s3, H2=L*(s-2*s2+s3), H3=3*s2-2*s3, H4=L*(-s2+s3);
    e.equivalent(0)+=w*N1*v.x();  e.equivalent(6)+=w*N2*v.x();
    e.equivalent(1)+=w*H1*v.y();  e.equivalent(5)+=w*H2*v.y();
    e.equivalent(7)+=w*H3*v.y();  e.equivalent(11)+=w*H4*v.y();
    // theta_y = -dw/dx, so the z-plane rotational shape functions change sign.
    e.equivalent(2)+=w*H1*v.z();  e.equivalent(4)-=w*H2*v.z();
    e.equivalent(8)+=w*H3*v.z();  e.equivalent(10)-=w*H4*v.z();
    e.force+=w*v;
    e.moment_node1+=w*Eigen::Vector3d(x,0,0).cross(v);
  }else{
    // Shape-function derivatives d/dx.
    const Real D1=(-6*s+6*s2)/L, D2=1-4*s+3*s2, D3=(6*s-6*s2)/L, D4=-2*s+3*s2;
    e.equivalent(3)+=w*N1*v.x();  e.equivalent(9)+=w*N2*v.x();       // torsion
    e.equivalent(1)+=w*D1*v.z();  e.equivalent(5)+=w*D2*v.z();       // M_z works through theta_z = dv/dx
    e.equivalent(7)+=w*D3*v.z();  e.equivalent(11)+=w*D4*v.z();
    e.equivalent(2)-=w*D1*v.y();  e.equivalent(4)+=w*D2*v.y();       // M_y works through theta_y = -dw/dx
    e.equivalent(8)-=w*D3*v.y();  e.equivalent(10)+=w*D4*v.y();
    e.moment_node1+=w*v;
  }
}

inline Eigen::Vector3d localDirection(LoadAxes axes,int direction,const Eigen::Matrix3d& R){
  return axes==LoadAxes::Local ? Eigen::Vector3d::Unit(direction) : Eigen::Vector3d(R.col(direction));
}

// R: rows are the member's local x, y, z axes in global coordinates. L: member length.
// The integrands are polynomials of degree <= 4, so 3-point Gauss-Legendre is exact.
inline MemberLoadEffect memberLoadEffect(const MemberLoad& ld,const Eigen::Matrix3d& R,Real L){
  const Real xa=ld.start*L, xb=ld.end*L, h=xb-xa;
  const Eigen::Vector3d dir=localDirection(ld.axes,ld.direction,R);
  static const Real gp[3]={-0.7745966692414834,0.0,0.7745966692414834};
  static const Real gw[3]={5.0/9.0,8.0/9.0,5.0/9.0};
  MemberLoadEffect e;
  for(int g=0;g<3;++g){
    const Real t=0.5*(gp[g]+1), x=xa+t*h, wq=0.5*h*gw[g];
    const Real q=ld.w1+(ld.w2-ld.w1)*t;
    addLocalAction(e,x,L,dir*q,ld.kind,wq);
  }
  return e;
}

inline MemberLoadEffect pointLoadEffect(const MemberPointLoad& ld,const Eigen::Matrix3d& R,Real L){
  MemberLoadEffect e;
  addLocalAction(e,ld.position*L,L,localDirection(ld.axes,ld.direction,R)*ld.value,ld.kind,1.0);
  return e;
}

} // namespace detail
} // namespace kopruq
