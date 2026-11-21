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
// - Equivalent nodal loads are the consistent load vector f = integral(N^T q dx)
//   with the element's own shape functions, which equals the negated exact
//   fixed-end forces. Element end forces are recovered as k*u - f.
//   2026-11-21: for a Timoshenko member (shear area > 0) the bending shape
//   functions are the interdependent interpolation functions of the
//   Timoshenko beam (Friedman & Kosmatka 1993; Reddy), parameter
//   Phi = 12 E I / (G As L^2) of the bending plane, matching Beam3D's
//   stiffness; they reduce to the cubic Hermite functions at Phi = 0. Before,
//   the Hermite functions were used for every member, which is exact for
//   Timoshenko members only for full-length uniform transverse loads. Evidence
//   (verification model 23, uniform moment m on a PSC member, Phi = 0.53):
//   MIDAS's end forces equal the exact Timoshenko fixed-end values
//   V = m/(1+Phi), M = m L Phi/(2(1+Phi)); the Hermite vector gives V = m, M = 0.
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
// phiy/phiz: Timoshenko shear parameters of the bending planes -- phiy for v / theta_z (local y
// deflection, Iz, Asy), phiz for w / theta_y (Iy, Asz); 0 = Euler-Bernoulli. H: deflection shape
// functions (forces work through v, w); G: rotation shape functions (distributed/concentrated
// moments work through theta). With mu = 1/(1+phi), s = x/L:
//   H1 = mu(2s^3 - 3s^2 - phi s + 1 + phi)       H2 = L mu(s^3 - (2 + phi/2) s^2 + (1 + phi/2) s)
//   H3 = mu(-2s^3 + 3s^2 + phi s)                H4 = L mu(s^3 - (1 - phi/2) s^2 - (phi/2) s)
//   G1 = 6 mu (s^2 - s)/L                        G2 = mu(3s^2 - (4 + phi) s + 1 + phi)
//   G3 = -6 mu (s^2 - s)/L                       G4 = mu(3s^2 - (2 - phi) s)
inline void addLocalAction(MemberLoadEffect& e,Real x,Real L,const Eigen::Vector3d& v,MemberLoadKind kind,Real w,Real phiy=0,Real phiz=0){
  const Real s=x/L, s2=s*s, s3=s2*s;
  const Real N1=1-s, N2=s;
  auto H=[&](Real f,int i){const Real mu=1/(1+f);switch(i){case 1:return mu*(2*s3-3*s2-f*s+1+f);case 2:return L*mu*(s3-(2+f/2)*s2+(1+f/2)*s);case 3:return mu*(-2*s3+3*s2+f*s);default:return L*mu*(s3-(1-f/2)*s2-(f/2)*s);}};
  auto G=[&](Real f,int i){const Real mu=1/(1+f);switch(i){case 1:return 6*mu/L*(s2-s);case 2:return mu*(3*s2-(4+f)*s+1+f);case 3:return -6*mu/L*(s2-s);default:return mu*(3*s2-(2-f)*s);}};
  if(kind==MemberLoadKind::Force){
    e.equivalent(0)+=w*N1*v.x();  e.equivalent(6)+=w*N2*v.x();
    e.equivalent(1)+=w*H(phiy,1)*v.y();  e.equivalent(5)+=w*H(phiy,2)*v.y();
    e.equivalent(7)+=w*H(phiy,3)*v.y();  e.equivalent(11)+=w*H(phiy,4)*v.y();
    e.equivalent(2)+=w*H(phiz,1)*v.z();  e.equivalent(4)-=w*H(phiz,2)*v.z();
    e.equivalent(8)+=w*H(phiz,3)*v.z();  e.equivalent(10)-=w*H(phiz,4)*v.z();
    e.force+=w*v;
    e.moment_node1+=w*Eigen::Vector3d(x,0,0).cross(v);
  }else{
    e.equivalent(3)+=w*N1*v.x();  e.equivalent(9)+=w*N2*v.x();
    e.equivalent(1)+=w*G(phiy,1)*v.z();  e.equivalent(5)+=w*G(phiy,2)*v.z();
    e.equivalent(7)+=w*G(phiy,3)*v.z();  e.equivalent(11)+=w*G(phiy,4)*v.z();
    e.equivalent(2)-=w*G(phiz,1)*v.y();  e.equivalent(4)+=w*G(phiz,2)*v.y();
    e.equivalent(8)-=w*G(phiz,3)*v.y();  e.equivalent(10)+=w*G(phiz,4)*v.y();
    e.moment_node1+=w*v;
  }
}

inline Eigen::Vector3d localDirection(LoadAxes axes,int direction,const Eigen::Matrix3d& R){
  return axes==LoadAxes::Local ? Eigen::Vector3d::Unit(direction) : Eigen::Vector3d(R.col(direction));
}

// R: rows are the member's local x, y, z axes in global coordinates. L: member length.
// The integrands are polynomials of degree <= 4, so 3-point Gauss-Legendre is exact.
inline MemberLoadEffect memberLoadEffect(const MemberLoad& ld,const Eigen::Matrix3d& R,Real L,Real phiy=0,Real phiz=0){
  const Real xa=ld.start*L, xb=ld.end*L, h=xb-xa;
  const Eigen::Vector3d dir=localDirection(ld.axes,ld.direction,R);
  static const Real gp[3]={-0.7745966692414834,0.0,0.7745966692414834};
  static const Real gw[3]={5.0/9.0,8.0/9.0,5.0/9.0};
  MemberLoadEffect e;
  for(int g=0;g<3;++g){
    const Real t=0.5*(gp[g]+1), x=xa+t*h, wq=0.5*h*gw[g];
    const Real q=ld.w1+(ld.w2-ld.w1)*t;
    addLocalAction(e,x,L,dir*q,ld.kind,wq,phiy,phiz);
  }
  return e;
}

inline MemberLoadEffect pointLoadEffect(const MemberPointLoad& ld,const Eigen::Matrix3d& R,Real L,Real phiy=0,Real phiz=0){
  MemberLoadEffect e;
  addLocalAction(e,ld.position*L,L,localDirection(ld.axes,ld.direction,R)*ld.value,ld.kind,1.0,phiy,phiz);
  return e;
}

} // namespace detail
} // namespace kopruq
