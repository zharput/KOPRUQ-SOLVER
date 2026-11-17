#pragma once
// Undamped free-vibration (modal) analysis: Kff*phi = omega^2*Mff*phi on the
// free DOFs (restrained DOFs removed exactly as in LinearStaticSolver), mass
// matrix condensed the same way stiffness is for released member ends
// (Releases.hpp). Mode shapes are mass-normalized (phi^T Mff phi = 1) and
// scattered to full global DOF vectors with zero at restrained DOFs.
#include "LinearStaticSolver.hpp"
#include "../assembly/RigidLink.hpp"
#include <Eigen/Cholesky>
#include <Eigen/Eigenvalues>
#include <algorithm>

namespace kopruq {

struct ModeResult { Real angular_frequency_rad_s{}, frequency_hz{}; Eigen::VectorXd shape; };
struct ModalResults { SolverStatus status; std::vector<ModeResult> modes; std::size_t total_dofs{}, free_dofs{}, fixed_dofs{}; };

// Consistent (ModalSolver's long-standing default, independently verified R1-R4/Phase 6.1) vs
// Lumped (2026-11-17, added after an independent diagnostic confirmed MIDAS NX's own default
// eigenvalue analysis uses a classic translational-only lumped mass -- see MassMatrix.hpp's
// lumpedMassMatrix comment for the evidence). Neither is "more correct" in general; Lumped exists
// specifically to let a caller reproduce MIDAS's own convention when cross-checking against it.
enum class MassMatrixType { Consistent, Lumped };

class ModalSolver { public:
  // numModes <= 0 returns every free-DOF mode.
  static ModalResults solve(const Model& m, const Restraints& rs, int numModes = 6, MassMatrixType massType = MassMatrixType::Consistent) {
    ModalResults out;
    try {
      DofManager dm(m);
      if (!m.rigid_links.empty()) {
        if (massType == MassMatrixType::Lumped) { out.status = {false, "Lumped mass is not yet supported together with rigid links", SolverStatusCode::InvalidModel}; return out; }
        return solveWithRigidLinks(m, rs, numModes, dm);
      }
      out.total_dofs = dm.dof_count();
      if (rs.empty()) { out.status = {false, "no restraints", SolverStatusCode::NoRestraints}; return out; }
      auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
      auto b = MatrixAssembler::classify(dm, all);
      out.free_dofs = b.free.size(); out.fixed_dofs = b.fixed.size();
      if (b.free.empty()) { out.status = {false, "no free degrees of freedom", SolverStatusCode::NoRestraints}; return out; }
      Eigen::SparseMatrix<Real> K = MatrixAssembler::assemble(m);
      if (massType == MassMatrixType::Lumped) {
        out = eigensolveLumped(K, MatrixAssembler::assembleLumpedMass(m), b.free, numModes, dm.dof_count());
      } else {
        Eigen::SparseMatrix<Real> M = MatrixAssembler::assembleMass(m);
        Eigen::SparseMatrix<Real> Kff = LinearStaticSolver::reduceKff(K, b.free), Mff = LinearStaticSolver::reduceKff(M, b.free);
        out = eigensolve(Kff, Mff, b.free, numModes, dm.dof_count());
      }
      out.total_dofs = dm.dof_count(); out.free_dofs = b.free.size(); out.fixed_dofs = b.fixed.size();
    } catch (const std::exception& e) { out.status = {false, e.what(), SolverStatusCode::InvalidModel}; }
    return out;
  }

 private:
  // Shared dense symmetric generalized eigensolve K*phi = omega^2*M*phi (already-reduced,
  // nonsingular, positive-definite M). Returns the `n` smallest eigenpairs (lambda=omega^2,
  // Y=mass-orthonormalized-y, Linv=the Cholesky factor used, so callers in the mass-normalized
  // phi-space can recover phi = Linv^T * Y themselves).
  struct EigenResult { SolverStatus status; Eigen::VectorXd lambda; Eigen::MatrixXd Linv, Y; };
  static EigenResult symmetricGeneralizedEigen(Eigen::MatrixXd Kd, Eigen::MatrixXd Md) {
    EigenResult r;
    Kd = 0.5 * (Kd + Kd.transpose()); Md = 0.5 * (Md + Md.transpose());
    Eigen::LLT<Eigen::MatrixXd> mchol(Md);
    if (mchol.info() != Eigen::Success) { r.status = {false, "mass matrix is not positive definite on the free degrees of freedom (a free DOF has no mass -- check unit_weight)", SolverStatusCode::SingularSystem}; return r; }
    // Standard transformation to a symmetric standard eigenproblem: L L^T = M, A = L^-1 K L^-T, A y = omega^2 y, phi = L^-T y.
    r.Linv = Eigen::MatrixXd::Identity(Kd.rows(), Kd.rows());
    mchol.matrixL().solveInPlace(r.Linv);
    Eigen::MatrixXd A = r.Linv * Kd * r.Linv.transpose();
    A = 0.5 * (A + A.transpose());
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(A);
    if (es.info() != Eigen::Success) { r.status = {false, "eigenvalue solve failed to converge", SolverStatusCode::NumericalFailure}; return r; }
    r.lambda = es.eigenvalues();
    r.Y = es.eigenvectors();
    r.status = {true, "ok", SolverStatusCode::Success};
    return r;
  }

  // Shared eigensolve on an already-reduced (free-DOF) Kff/Mff pair; `scatter` maps each
  // reduced-free index back to a full-DOF-space index for building a mode's raw shape vector
  // (still in "pre-rigid-link" full space when called from solveWithRigidLinks -- the caller
  // expands that through T afterwards, same as LinearStaticSolver's recover/expand split).
  static ModalResults eigensolve(const Eigen::SparseMatrix<Real>& Kff, const Eigen::SparseMatrix<Real>& Mff,
                                  const std::vector<std::size_t>& scatter, int numModes, std::size_t shapeSize) {
    ModalResults out;
    EigenResult eig = symmetricGeneralizedEigen(Eigen::MatrixXd(Kff), Eigen::MatrixXd(Mff));
    if (!eig.status.success) { out.status = eig.status; return out; }
    int n = numModes > 0 ? std::min<int>(numModes, static_cast<int>(eig.lambda.size())) : static_cast<int>(eig.lambda.size());
    out.modes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      Real lam = std::max(Real(0), eig.lambda(i));   // round-off can make a rigid-body-adjacent eigenvalue slightly negative
      ModeResult mode; mode.angular_frequency_rad_s = std::sqrt(lam); mode.frequency_hz = mode.angular_frequency_rad_s / (2 * 3.14159265358979323846);
      Eigen::VectorXd phiFree = eig.Linv.transpose() * eig.Y.col(i);   // mass-normalized: phi^T Mff phi = 1
      mode.shape = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(shapeSize));
      for (std::size_t k = 0; k < scatter.size(); ++k) mode.shape(static_cast<Eigen::Index>(scatter[k])) = phiFree(static_cast<Eigen::Index>(k));
      out.modes.push_back(std::move(mode));
    }
    out.status = {true, "modal analysis completed", SolverStatusCode::Success};
    return out;
  }

  // Lumped-mass eigensolve: `Mfree` (the lumped mass reduced to free DOFs) is singular on every
  // rotational DOF (lumpedMassMatrix puts zero mass there, by design -- see its own comment).
  // Splits `free` into "massy" (nonzero lumped mass, i.e. translational) and "massless"
  // (rotational) subsets, statically condenses the massless ones out of K (Guyan reduction,
  // K_cond = K_aa - K_ar*K_rr^-1*K_ra, the same technique RigidLink.hpp's rigid-link slaves and
  // Releases.hpp's end releases both already use elsewhere in this codebase), solves the
  // well-posed (Kcond, diag(massy mass)) generalized eigenproblem, then recovers each massless
  // DOF's own mode-shape value by back-substitution (phi_r = -K_rr^-1*K_ra*phi_a) -- never left
  // at zero, since a rotational DOF's displacement is real even though it carries no mass.
  // Mass-normalization is exact and unaffected by this split: phi^T M_full phi = phi_a^T M_aa
  // phi_a (M_full is zero on every row/column phi_r touches), so normalizing against M_aa alone
  // already normalizes against the full mass matrix.
  static ModalResults eigensolveLumped(const Eigen::SparseMatrix<Real>& K, const Eigen::SparseMatrix<Real>& Mfull,
                                        const std::vector<std::size_t>& free, int numModes, std::size_t shapeSize) {
    ModalResults out;
    std::vector<std::size_t> active, massless; // both index into `free`'s own global DOF indices
    for (auto idx : free) ((Mfull.coeff(static_cast<int>(idx), static_cast<int>(idx)) > 0) ? active : massless).push_back(idx);
    if (active.empty()) { out.status = {false, "mass matrix is not positive definite on the free degrees of freedom (a free DOF has no mass -- check unit_weight)", SolverStatusCode::SingularSystem}; return out; }

    auto denseBlock = [&](const std::vector<std::size_t>& rows, const std::vector<std::size_t>& cols) {
      Eigen::MatrixXd block(rows.size(), cols.size());
      for (std::size_t i = 0; i < rows.size(); ++i)
        for (std::size_t j = 0; j < cols.size(); ++j)
          block(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) = K.coeff(static_cast<int>(rows[i]), static_cast<int>(cols[j]));
      return block;
    };
    const Eigen::MatrixXd Kaa = denseBlock(active, active);
    Eigen::MatrixXd Kcond = Kaa;
    Eigen::MatrixXd Krr_inv_Kra; // only needed (and only computed) when there are massless DOFs to back-substitute
    if (!massless.empty()) {
      const Eigen::MatrixXd Kar = denseBlock(active, massless), Krr = denseBlock(massless, massless), Kra = denseBlock(massless, active);
      Eigen::LDLT<Eigen::MatrixXd> krrFac(Krr);
      if (krrFac.info() != Eigen::Success) { out.status = {false, "the massless (rotational) degrees of freedom could not be statically condensed (singular local stiffness)", SolverStatusCode::SingularSystem}; return out; }
      Krr_inv_Kra = krrFac.solve(Kra);
      Kcond -= Kar * Krr_inv_Kra;
    }

    Eigen::VectorXd mDiag(static_cast<Eigen::Index>(active.size()));
    for (std::size_t i = 0; i < active.size(); ++i) mDiag(static_cast<Eigen::Index>(i)) = Mfull.coeff(static_cast<int>(active[i]), static_cast<int>(active[i]));
    Eigen::MatrixXd Maa = mDiag.asDiagonal();

    EigenResult eig = symmetricGeneralizedEigen(Kcond, Maa);
    if (!eig.status.success) { out.status = eig.status; return out; }
    int n = numModes > 0 ? std::min<int>(numModes, static_cast<int>(eig.lambda.size())) : static_cast<int>(eig.lambda.size());
    out.modes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
      Real lam = std::max(Real(0), eig.lambda(i));
      ModeResult mode; mode.angular_frequency_rad_s = std::sqrt(lam); mode.frequency_hz = mode.angular_frequency_rad_s / (2 * 3.14159265358979323846);
      const Eigen::VectorXd phiActive = eig.Linv.transpose() * eig.Y.col(i); // mass-normalized against Maa (== full M, see comment above)
      mode.shape = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(shapeSize));
      for (std::size_t k = 0; k < active.size(); ++k) mode.shape(static_cast<Eigen::Index>(active[k])) = phiActive(static_cast<Eigen::Index>(k));
      if (!massless.empty()) {
        const Eigen::VectorXd phiMassless = -Krr_inv_Kra * phiActive;
        for (std::size_t k = 0; k < massless.size(); ++k) mode.shape(static_cast<Eigen::Index>(massless[k])) = phiMassless(static_cast<Eigen::Index>(k));
      }
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
