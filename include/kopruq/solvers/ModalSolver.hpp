#pragma once
// Undamped free-vibration (modal) analysis: Kff*phi = omega^2*Mff*phi on the
// free DOFs (restrained DOFs removed exactly as in LinearStaticSolver), mass
// matrix condensed the same way stiffness is for released member ends
// (Releases.hpp). Mode shapes are mass-normalized (phi^T Mff phi = 1) and
// scattered to full global DOF vectors with zero at restrained DOFs.
#include "LinearStaticSolver.hpp"
#include "../assembly/RigidLink.hpp"
#include <Eigen/Eigenvalues>
#include <algorithm>

namespace kopruq {

struct ModeResult { Real angular_frequency_rad_s{}, frequency_hz{}; Eigen::VectorXd shape; };
struct ModalResults { SolverStatus status; std::vector<ModeResult> modes; std::size_t total_dofs{}, free_dofs{}, fixed_dofs{}; };

class ModalSolver { public:
  // numModes <= 0 returns every free-DOF mode.
  static ModalResults solve(const Model& m, const Restraints& rs, int numModes = 6) {
    ModalResults out;
    try {
      DofManager dm(m);
      if (!m.rigid_links.empty()) return solveWithRigidLinks(m, rs, numModes, dm);
      out.total_dofs = dm.dof_count();
      if (rs.empty()) { out.status = {false, "no restraints", SolverStatusCode::NoRestraints}; return out; }
      auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
      auto b = MatrixAssembler::classify(dm, all);
      out.free_dofs = b.free.size(); out.fixed_dofs = b.fixed.size();
      if (b.free.empty()) { out.status = {false, "no free degrees of freedom", SolverStatusCode::NoRestraints}; return out; }
      Eigen::SparseMatrix<Real> K = MatrixAssembler::assemble(m), M = MatrixAssembler::assembleMass(m);
      Eigen::SparseMatrix<Real> Kff = LinearStaticSolver::reduceKff(K, b.free), Mff = LinearStaticSolver::reduceKff(M, b.free);
      out = eigensolve(Kff, Mff, b.free, numModes, dm.dof_count());
      out.total_dofs = dm.dof_count(); out.free_dofs = b.free.size(); out.fixed_dofs = b.fixed.size();
    } catch (const std::exception& e) { out.status = {false, e.what(), SolverStatusCode::InvalidModel}; }
    return out;
  }

 private:
  // Shared eigensolve on an already-reduced (free-DOF) Kff/Mff pair; `scatter` maps each
  // reduced-free index back to a full-DOF-space index for building a mode's raw shape vector
  // (still in "pre-rigid-link" full space when called from solveWithRigidLinks -- the caller
  // expands that through T afterwards, same as LinearStaticSolver's recover/expand split).
  static ModalResults eigensolve(const Eigen::SparseMatrix<Real>& Kff, const Eigen::SparseMatrix<Real>& Mff,
                                  const std::vector<std::size_t>& scatter, int numModes, std::size_t shapeSize) {
    ModalResults out;
    Eigen::MatrixXd Kd(Kff), Md(Mff);
    Kd = 0.5 * (Kd + Kd.transpose()); Md = 0.5 * (Md + Md.transpose());
    Eigen::LLT<Eigen::MatrixXd> mchol(Md);
    if (mchol.info() != Eigen::Success) { out.status = {false, "mass matrix is not positive definite on the free degrees of freedom (a free DOF has no mass -- check unit_weight)", SolverStatusCode::SingularSystem}; return out; }
    // Standard transformation to a symmetric standard eigenproblem: L L^T = M, A = L^-1 K L^-T, A y = omega^2 y, phi = L^-T y.
    Eigen::MatrixXd Linv = Eigen::MatrixXd::Identity(Kd.rows(), Kd.rows());
    mchol.matrixL().solveInPlace(Linv);
    Eigen::MatrixXd A = Linv * Kd * Linv.transpose();
    A = 0.5 * (A + A.transpose());
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(A);
    if (es.info() != Eigen::Success) { out.status = {false, "eigenvalue solve failed to converge", SolverStatusCode::NumericalFailure}; return out; }
    Eigen::VectorXd lambda = es.eigenvalues();
    Eigen::MatrixXd Y = es.eigenvectors();
    int n = numModes > 0 ? std::min<int>(numModes, static_cast<int>(lambda.size())) : static_cast<int>(lambda.size());
    out.modes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      Real lam = std::max(Real(0), lambda(i));   // round-off can make a rigid-body-adjacent eigenvalue slightly negative
      ModeResult mode; mode.angular_frequency_rad_s = std::sqrt(lam); mode.frequency_hz = mode.angular_frequency_rad_s / (2 * 3.14159265358979323846);
      Eigen::VectorXd phiFree = Linv.transpose() * Y.col(i);   // mass-normalized: phi^T Mff phi = 1
      mode.shape = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(shapeSize));
      for (std::size_t k = 0; k < scatter.size(); ++k) mode.shape(static_cast<Eigen::Index>(scatter[k])) = phiFree(static_cast<Eigen::Index>(k));
      out.modes.push_back(std::move(mode));
    }
    out.status = {true, "modal analysis completed", SolverStatusCode::Success};
    return out;
  }

  // Same algorithm as solve(), with Kff/Mff built in the rigid-link-REDUCED DOF space
  // (RigidLinkReduction, see RigidLink.hpp): K/M are assembled in full DOF space exactly as
  // without rigid links, only the eigensolve step operates on T^T(.)T-reduced matrices, and the
  // resulting reduced-space mode shape is expanded back (`red.expand`) to full DOF space, giving
  // each slave node's 6 DOFs via the same rigid kinematics LinearStaticSolver uses -- never zero.
  // Mass-normalization survives the expansion exactly: phi_full^T M_full phi_full =
  // phiReduced^T (T^T M_full T) phiReduced = phiReduced^T Mmid phiReduced = phiFree^T Mff phiFree = 1,
  // since Mmid = red.reduceMatrix(M) by construction.
  static ModalResults solveWithRigidLinks(const Model& m, const Restraints& rs, int numModes, const DofManager& dm) {
    ModalResults out;
    out.total_dofs = dm.dof_count();
    try {
      if (rs.empty()) { out.status = {false, "no restraints", SolverStatusCode::NoRestraints}; return out; }
      auto red = RigidLinkReduction::build(m, dm);
      auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
      for (const auto& slave : red.slaveNodes) { auto it = all.find(slave); if (it != all.end()) for (bool flag : it->second) if (flag) throw AssemblyError(AssemblyErrorCode::MissingNode, "rigid-link slave node " + std::to_string(slave) + " cannot carry its own restraints"); }
      auto allRetained = all; for (const auto& slave : red.slaveNodes) allRetained.erase(slave);
      auto bFull = MatrixAssembler::classify(dm, allRetained);
      std::vector<std::size_t> freeReduced;
      for (auto fi : bFull.free) freeReduced.push_back(red.fullToReduced(fi));
      out.free_dofs = freeReduced.size(); out.fixed_dofs = bFull.fixed.size();
      if (freeReduced.empty()) { out.status = {false, "no free degrees of freedom", SolverStatusCode::NoRestraints}; return out; }
      Eigen::SparseMatrix<Real> K = MatrixAssembler::assemble(m), M = MatrixAssembler::assembleMass(m);
      Eigen::SparseMatrix<Real> Kmid = red.reduceMatrix(K), Mmid = red.reduceMatrix(M);
      Eigen::SparseMatrix<Real> Kff = LinearStaticSolver::reduceKff(Kmid, freeReduced), Mff = LinearStaticSolver::reduceKff(Mmid, freeReduced);
      ModalResults reduced = eigensolve(Kff, Mff, freeReduced, numModes, red.reduced_count());
      if (!reduced.status.success) { out.status = reduced.status; return out; }
      out.modes.reserve(reduced.modes.size());
      for (auto& mode : reduced.modes) { mode.shape = red.expand(mode.shape); out.modes.push_back(std::move(mode)); }
      out.status = reduced.status;
    } catch (const std::exception& e) { out.status = {false, e.what(), SolverStatusCode::InvalidModel}; }
    return out;
  }
};

} // namespace kopruq
