#pragma once
// Load definitions and consistent equivalent nodal loads for Beam3D members.
//
// Conventions (approved for Phase 4.1, 2026-10-16):
// - Member loads are distributed forces in kN/m, uniform or linearly varying
//   over a segment [start, end] given as fractions of the member length
//   (0 = node1, 1 = node2), in member-local or global axes.
// - Global-axis intensities act per unit of true member length (not per
//   horizontal projection).
// - Equivalent nodal loads are the consistent Euler-Bernoulli load vector
//   f = integral(N^T q dx) with the element's cubic Hermite / linear shape
//   functions, which equals the negated exact fixed-end forces. Element end
//   forces are recovered as k*u - f.
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

struct MemberLoad {
  Id element{};
  LoadAxes axes{LoadAxes::Local};
  int direction{};          // 0, 1, 2 = x, y, z of the chosen axes
  Real w1{}, w2{};          // intensity at start and end of the loaded segment [kN/m]
  Real start{0}, end{1};    // loaded segment as fractions of member length
};

struct LoadCase {
  std::string name;
  std::vector<NodalLoad> nodal;
  std::vector<MemberLoad> member;
  Real self_weight{};       // factor on self-weight; 0 = no self-weight
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

// R: rows are the member's local x, y, z axes in global coordinates. L: member length.
// The integrands are polynomials of degree <= 4, so 3-point Gauss-Legendre is exact.
inline MemberLoadEffect memberLoadEffect(const MemberLoad& ld,const Eigen::Matrix3d& R,Real L){
  const Real xa=ld.start*L, xb=ld.end*L, h=xb-xa;
  const Eigen::Vector3d dir = ld.axes==LoadAxes::Local ? Eigen::Vector3d::Unit(ld.direction)
                                                       : Eigen::Vector3d(R.col(ld.direction));
  static const Real gp[3]={-0.7745966692414834,0.0,0.7745966692414834};
  static const Real gw[3]={5.0/9.0,8.0/9.0,5.0/9.0};
  MemberLoadEffect e;
  for(int g=0;g<3;++g){
    const Real t=0.5*(gp[g]+1), x=xa+t*h, wq=0.5*h*gw[g];
    const Real q=ld.w1+(ld.w2-ld.w1)*t;
    const Eigen::Vector3d qv=dir*q;
    const Real s=x/L, s2=s*s, s3=s2*s;
    const Real N1=1-s, N2=s;
    const Real H1=1-3*s2+2*s3, H2=L*(s-2*s2+s3), H3=3*s2-2*s3, H4=L*(-s2+s3);
    e.equivalent(0)+=wq*N1*qv.x();  e.equivalent(6)+=wq*N2*qv.x();
    e.equivalent(1)+=wq*H1*qv.y();  e.equivalent(5)+=wq*H2*qv.y();
    e.equivalent(7)+=wq*H3*qv.y();  e.equivalent(11)+=wq*H4*qv.y();
    // theta_y = -dw/dx, so the z-plane rotational shape functions change sign.
    e.equivalent(2)+=wq*H1*qv.z();  e.equivalent(4)-=wq*H2*qv.z();
    e.equivalent(8)+=wq*H3*qv.z();  e.equivalent(10)-=wq*H4*qv.z();
    e.force+=wq*qv;
    e.moment_node1+=wq*Eigen::Vector3d(x,0,0).cross(qv);
  }
  return e;
}

} // namespace detail
} // namespace kopruq
