#pragma once
// Beam end releases by static condensation of the member-local 12x12 system.
//
// With a = retained and r = released local DOFs, the released end forces are
// zero, so k_rr u_r + k_ra u_a = f_r and
//   K_c = K_aa - K_ar K_rr^-1 K_ra,   f_c = f_a - K_ar K_rr^-1 f_r,
//   u_r = K_rr^-1 (f_r - K_ra u_a),
// where f are the member loads' consistent equivalent nodal loads. K_c and
// f_c are returned as full 12x12 / 12-vectors with zero released rows and
// columns, so they drop into the unreleased assembly unchanged.
#include "Beam3D.hpp"
#include <Eigen/Eigenvalues>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace kopruq {

using Releases = std::array<bool,12>;
using Beam3DVector = Eigen::Matrix<Real,12,1>;

inline bool anyReleased(const Releases& r) { return std::any_of(r.begin(), r.end(), [](bool b){ return b; }); }

namespace detail {

struct ReleasePartition { std::vector<int> a, r; Eigen::MatrixXd Krr, Kra; };

// Throws Beam3DError(UnstableRelease) when the released DOFs admit a zero-energy
// motion with the retained DOFs held (e.g. torsion or axial released at both
// ends): K_rr is then singular. Checked on the diagonally scaled K_rr so the
// test does not depend on units.
inline ReleasePartition partition(const Beam3DMatrix& k, const Releases& rel) {
  ReleasePartition p;
  for (int i = 0; i < 12; ++i) (rel[i] ? p.r : p.a).push_back(i);
  const auto nr = static_cast<Eigen::Index>(p.r.size()), na = static_cast<Eigen::Index>(p.a.size());
  p.Krr.resize(nr, nr); p.Kra.resize(nr, na);
  for (Eigen::Index i = 0; i < nr; ++i) {
    for (Eigen::Index j = 0; j < nr; ++j) p.Krr(i, j) = k(p.r[i], p.r[j]);
    for (Eigen::Index j = 0; j < na; ++j) p.Kra(i, j) = k(p.r[i], p.a[j]);
  }
  Eigen::VectorXd d = p.Krr.diagonal();
  bool stable = nr > 0 && (d.array() > 0).all();
  if (stable) {
    Eigen::VectorXd s = d.cwiseSqrt().cwiseInverse();
    Eigen::MatrixXd scaled = s.asDiagonal() * p.Krr * s.asDiagonal();
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(scaled, Eigen::EigenvaluesOnly);
    stable = es.info() == Eigen::Success && es.eigenvalues().minCoeff() > 1e-10 * es.eigenvalues().cwiseAbs().maxCoeff();
  }
  if (!stable) throw Beam3DError(Beam3DErrorCode::UnstableRelease, "end releases leave the element unstable (a rigid-body motion of the released DOFs is unrestrained)");
  return p;
}

} // namespace detail

inline Beam3DMatrix condensedStiffness(const Beam3DMatrix& k, const Releases& rel) {
  if (!anyReleased(rel)) return k;
  auto p = detail::partition(k, rel);
  Eigen::MatrixXd X = p.Krr.ldlt().solve(p.Kra);   // K_rr^-1 K_ra
  Beam3DMatrix kc = Beam3DMatrix::Zero();
  for (std::size_t i = 0; i < p.a.size(); ++i)
    for (std::size_t j = 0; j < p.a.size(); ++j) {
      Real corr = 0;
      for (std::size_t m = 0; m < p.r.size(); ++m) corr += k(p.a[i], p.r[m]) * X(static_cast<Eigen::Index>(m), static_cast<Eigen::Index>(j));
      kc(p.a[i], p.a[j]) = k(p.a[i], p.a[j]) - corr;
    }
  return 0.5 * (kc + kc.transpose());   // remove round-off asymmetry
}

inline Beam3DVector condensedLoad(const Beam3DMatrix& k, const Releases& rel, const Beam3DVector& f) {
  if (!anyReleased(rel)) return f;
  auto p = detail::partition(k, rel);
  Eigen::VectorXd fr(static_cast<Eigen::Index>(p.r.size()));
  for (std::size_t m = 0; m < p.r.size(); ++m) fr(static_cast<Eigen::Index>(m)) = f(p.r[m]);
  Eigen::VectorXd y = p.Krr.ldlt().solve(fr);       // K_rr^-1 f_r
  Beam3DVector fc = Beam3DVector::Zero();
  for (int i : p.a) {
    Real corr = 0;
    for (std::size_t m = 0; m < p.r.size(); ++m) corr += k(i, p.r[m]) * y(static_cast<Eigen::Index>(m));
    fc(i) = f(i) - corr;
  }
  return fc;
}

// Local displacements with the released entries replaced by the element's own
// end displacements, u_r = K_rr^-1 (f_r - K_ra u_a).
inline Beam3DVector completeDisplacements(const Beam3DMatrix& k, const Releases& rel, const Beam3DVector& u, const Beam3DVector& f) {
  if (!anyReleased(rel)) return u;
  auto p = detail::partition(k, rel);
  Eigen::VectorXd rhs(static_cast<Eigen::Index>(p.r.size()));
  for (std::size_t m = 0; m < p.r.size(); ++m) {
    Real s = f(p.r[m]);
    for (std::size_t j = 0; j < p.a.size(); ++j) s -= p.Kra(static_cast<Eigen::Index>(m), static_cast<Eigen::Index>(j)) * u(p.a[j]);
    rhs(static_cast<Eigen::Index>(m)) = s;
  }
  Eigen::VectorXd ur = p.Krr.ldlt().solve(rhs);
  Beam3DVector out = u;
  for (std::size_t m = 0; m < p.r.size(); ++m) out(p.r[m]) = ur(static_cast<Eigen::Index>(m));
  return out;
}

// Guyan (static) condensation of a mass matrix consistent with the stiffness
// condensation above: with T = K_rr^-1 K_ra (u_r = T u_a under static
// equilibrium, the same assumption K_c already makes),
//   M_c = M_aa - M_ar T - T^T M_ra + T^T M_rr T.
// Returned as a full 12x12 with zero released rows/columns, matching K_c.
inline Beam3DMatrix condensedMass(const Beam3DMatrix& k, const Beam3DMatrix& massMat, const Releases& rel) {
  if (!anyReleased(rel)) return massMat;
  auto p = detail::partition(k, rel);
  const auto na = static_cast<Eigen::Index>(p.a.size()), nr = static_cast<Eigen::Index>(p.r.size());
  Eigen::MatrixXd T = p.Krr.ldlt().solve(p.Kra);   // K_rr^-1 K_ra, na columns, nr rows
  Eigen::MatrixXd Maa(na, na), Mar(na, nr), Mrr(nr, nr);
  for (Eigen::Index i = 0; i < na; ++i) {
    for (Eigen::Index j = 0; j < na; ++j) Maa(i, j) = massMat(p.a[i], p.a[j]);
    for (Eigen::Index j = 0; j < nr; ++j) Mar(i, j) = massMat(p.a[i], p.r[j]);
  }
  for (Eigen::Index i = 0; i < nr; ++i) for (Eigen::Index j = 0; j < nr; ++j) Mrr(i, j) = massMat(p.r[i], p.r[j]);
  Eigen::MatrixXd Mc = Maa - Mar * T - T.transpose() * Mar.transpose() + T.transpose() * Mrr * T;
  Beam3DMatrix out = Beam3DMatrix::Zero();
  for (Eigen::Index i = 0; i < na; ++i) for (Eigen::Index j = 0; j < na; ++j) out(p.a[i], p.a[j]) = Mc(i, j);
  return 0.5 * (out + out.transpose());
}

} // namespace kopruq
