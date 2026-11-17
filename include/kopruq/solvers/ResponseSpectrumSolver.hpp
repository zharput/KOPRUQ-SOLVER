#pragma once
// Single-direction response spectrum analysis (RSA) on top of ModalSolver.
// The spectrum shape itself (code-specific: ASCE, Eurocode, site-specific,
// ...) is not computed here -- the caller supplies (period, spectral
// acceleration) points and this solver only interpolates between them, per
// the roadmap's "code-specific configuration isolated from solver kernel".
//
// Formulation (standard modal RSA, base excitation in global axis `direction`):
// - iota: unit ground displacement in `direction` at every FREE translational
//   DOF (0 at rotations and at restrained DOFs), matching the modal/
//   participating-mass convention most structural software reports (SAP2000,
//   ETABS): the free-DOF mode shapes phi_i are a *complete* Mff-orthonormal
//   basis of the free-DOF space, so Gamma_i = phi_i^T * M * iota and
//   total_mass = iota^T * M * iota satisfy sum_i Gamma_i^2 = total_mass
//   *exactly* when every free-DOF mode is included -- this is why iota must
//   be zero at restrained DOFs. (Setting iota to 1 there too, as a literal
//   "the ground moves the supports" base-excitation vector would, is also a
//   textbook-valid formulation, but it introduces a mass-coupling term
//   between the restrained and free DOFs through which no finite set of free-
//   DOF modes can ever capture 100% of "total_mass" -- found via the first
//   full test run, which could not previously execute past an unrelated CI
//   blocker; see docs/DEVELOPMENT_STATUS.md.)
// - Participation factor Gamma_i = phi_i^T * M * iota (M full; iota is zero
//   outside the free DOFs, so this equals phi_i_free^T * Mff * iota_free).
//   phi_i is mass-normalized by ModalSolver (phi_i^T Mff phi_i = 1).
// - Effective modal mass = Gamma_i^2; total_mass = iota^T M iota = the total
//   mass of the free DOFs alone in this direction (not the whole structure's
//   mass, since the restrained supports' own mass cannot move and is
//   excluded by construction).
// - Peak modal response: equivalent static force F_i = Gamma_i*Sa(T_i)*M*phi_i,
//   recovered via the already-verified LinearStaticSolver::recover() with
//   U_i = Gamma_i*Sa(T_i)/omega_i^2*phi_i and nodal loads F_i (both consistent
//   with each other because Kff phi_i = omega_i^2 Mff phi_i), giving an
//   exactly equilibrated per-mode result.
// - Modal combination: SRSS (square root of the sum of squares, the default) or CQC (Complete
//   Quadratic Combination, 2026-11-17) over every displacement/reaction/end-force component
//   independently. Both are peak *estimates*: they are not a single equilibrium state and
//   generally do not satisfy sum(F)=0 themselves -- that is expected, not a defect.
//
//   CQC (Der Kiureghian 1981, the standard formulation used by essentially every mainstream FEA
//   package -- SAP2000/ETABS/MIDAS all cite the same reference): combined = sqrt(sum_i sum_j
//   rho_ij * R_i * R_j), with the cross-modal correlation coefficient
//     rho_ij = 8*zeta_i*zeta_j*(zeta_i+r*zeta_j)*r^1.5 / ((1-r^2)^2 + 4*zeta_i*zeta_j*r*(1+r^2)
//              + 4*(zeta_i^2+zeta_j^2)*r^2),   r = omega_j/omega_i.
//   SRSS is the special case rho_ij = (i==j) (every off-diagonal cross term is zero) -- both are
//   implemented through the exact same "combined = sqrt(sum_i sum_j rho_ij*R_i*R_j)" code path
//   here, never two separate, potentially-diverging implementations.
//   This implementation uses a single, caller-supplied damping ratio for every mode (constant
//   damping) -- the common case, and the only one this project has real MIDAS reference data
//   for (verification/models/16's SPFC record uses one DRATIO for the whole spectrum function);
//   per-mode-varying damping is not supported (the formula above degenerates correctly to
//   zeta_i=zeta_j=zeta, so nothing here assumes equal damping as a simplification, it is simply
//   the only input provided).
#include "ModalSolver.hpp"
#include <algorithm>

namespace kopruq {

struct ResponseSpectrumPoint { Real period_s{}, spectral_acceleration{}; }; // period [s], Sa [m/s^2]
enum class ModalCombination { SRSS, CQC };
struct ResponseSpectrumInput {
  std::vector<ResponseSpectrumPoint> spectrum;
  int direction{};
  int num_modes{6};
  ModalCombination combination{ModalCombination::SRSS};
  Real damping_ratio{0.05}; // only used when combination == CQC
};

// Piecewise-linear interpolation; throws std::invalid_argument if period_s is outside [spectrum.front(), spectrum.back()]
// or the spectrum itself is malformed (fewer than 2 points, non-finite/non-positive period, non-finite/negative Sa,
// or periods not strictly increasing).
inline Real interpolateSpectrum(const std::vector<ResponseSpectrumPoint>& spectrum, Real period_s) {
  if (spectrum.size() < 2) throw std::invalid_argument("response spectrum must have at least 2 points");
  for (std::size_t i = 0; i < spectrum.size(); ++i) {
    const auto& p = spectrum[i];
    if (!std::isfinite(p.period_s) || p.period_s <= 0 || !std::isfinite(p.spectral_acceleration) || p.spectral_acceleration < 0)
      throw std::invalid_argument("response spectrum point is invalid");
    if (i > 0 && !(p.period_s > spectrum[i - 1].period_s)) throw std::invalid_argument("response spectrum periods must be strictly increasing");
  }
  if (period_s < spectrum.front().period_s || period_s > spectrum.back().period_s)
    throw std::invalid_argument("mode period is outside the response spectrum's range");
  for (std::size_t i = 1; i < spectrum.size(); ++i) {
    if (period_s > spectrum[i].period_s) continue;
    const Real t0 = spectrum[i - 1].period_s, t1 = spectrum[i].period_s;
    const Real a0 = spectrum[i - 1].spectral_acceleration, a1 = spectrum[i].spectral_acceleration;
    return a0 + (period_s - t0) / (t1 - t0) * (a1 - a0);
  }
  return spectrum.back().spectral_acceleration;
}

// Der Kiureghian (1981) cross-modal correlation coefficient for CQC; rho(omega,omega,zeta) == 1
// always (r=1 limit). `zeta` must be finite and > 0 -- callers validate this once, not per call.
inline Real cqcCorrelation(Real omega_i, Real omega_j, Real zeta) {
  if (omega_i == omega_j) return 1.0; // exact r=1 limit, avoids 0/0 from floating-point r very close to but not exactly 1
  const Real r = omega_j / omega_i;
  const Real num = 8 * zeta * zeta * (1 + r) * std::pow(r, 1.5);
  const Real den = (1 - r * r) * (1 - r * r) + 4 * zeta * zeta * r * (1 + r) * (1 + r);
  return num / den;
}

struct ModalContribution { Real period_s{}, frequency_hz{}, participation_factor{}, effective_mass{}, mass_participation_ratio{}; AnalysisResults results; };
struct NodalPeakResponse { Id node_id{}; DofVector displacement{}, reaction{}; };
struct ElementPeakResponse { Id element_id{}; std::array<Real,12> local_end_force{}, global_end_force{}; };
struct ResponseSpectrumResults {
  SolverStatus status;
  std::vector<ModalContribution> modes;
  std::vector<NodalPeakResponse> combined_nodes;
  std::vector<ElementPeakResponse> combined_elements;
  Real total_mass{}, total_mass_participation_ratio{};
};

class ResponseSpectrumSolver { public:
  // `massType`: see ModalSolver.hpp's MassMatrixType. Threaded through to BOTH the underlying
  // modal solve AND this solver's own use of the mass matrix (iota/Miota, per-mode equivalent
  // static force) -- using Consistent for one and Lumped for the other would be internally
  // inconsistent (Gamma_i/total_mass would no longer correspond to the same physical mass
  // distribution `phi_i` was computed against).
  static ResponseSpectrumResults solve(const Model& m, const Restraints& rs, const ResponseSpectrumInput& input,
                                        MassMatrixType massType = MassMatrixType::Consistent) {
    ResponseSpectrumResults out;
    try {
      if (input.direction < 0 || input.direction > 2) throw std::invalid_argument("direction must be 0 (x), 1 (y) or 2 (z)");
      auto modal = ModalSolver::solve(m, rs, input.num_modes, massType);
      if (!modal.status.success) { out.status = modal.status; return out; }

      DofManager dm(m);
      Eigen::SparseMatrix<Real> Mfull = massType == MassMatrixType::Lumped ? MatrixAssembler::assembleLumpedMass(m) : MatrixAssembler::assembleMass(m);

      auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
      // A rigid-link slave node's 6 DOFs are not independent (see RigidLink.hpp) -- they must be
      // excluded here exactly as LinearStaticSolver::solveCasesWithRigidLinks excludes them from
      // `allRetained` before classify, or they would wrongly appear as extra "free" DOFs with
      // their own iota=1 entries, double-counting mass already captured through the master's
      // modal response (ModalSolver::solve has already expanded each mode's shape through the
      // same rigid kinematics, so the slave DOFs' motion is already fully represented in `phi`).
      std::set<Id> slaveNodes;
      if (!m.rigid_links.empty()) slaveNodes = RigidLinkReduction::build(m, dm).slaveNodes;
      auto allRetained = all; for (const auto& slave : slaveNodes) allRetained.erase(slave);
      auto free = MatrixAssembler::classify(dm, allRetained).free;

      // iota: 1 at every FREE translational DOF matching `direction` (DOF index mod 6:
      // 0=UX, 1=UY, 2=UZ), 0 everywhere else, including restrained DOFs -- see the
      // formulation note above.
      Eigen::VectorXd iota = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(dm.dof_count()));
      for (auto idx : free) if (idx % 6 == static_cast<std::size_t>(input.direction)) iota(static_cast<Eigen::Index>(idx)) = 1.0;
      Eigen::VectorXd Miota = Mfull * iota;
      out.total_mass = iota.dot(Miota);

      // Per-mode values are collected first (one vector<Real> per DOF/component, length ==
      // number of modes) and combined only after the loop -- CQC needs every mode's value
      // simultaneously (cross terms), unlike SRSS's simple running sum of squares.
      std::map<Id,std::array<std::vector<Real>,6>> dispByMode, reacByMode;
      std::map<Id,std::array<std::vector<Real>,12>> localForceByMode, forceByMode;
      for (const auto& n : m.nodes) { dispByMode[n.id] = {}; reacByMode[n.id] = {}; }
      for (const auto& e : m.beams) { localForceByMode[e.id] = {}; forceByMode[e.id] = {}; }

      for (const auto& mode : modal.modes) {
        const Real omega2 = mode.angular_frequency_rad_s * mode.angular_frequency_rad_s;
        if (!(omega2 > 0)) throw std::invalid_argument("a mode has zero or near-zero frequency (an insufficiently restrained model / mechanism); response spectrum analysis requires strictly positive frequencies");
        const Real Gamma = mode.shape.dot(Miota);
        const Real T = 1.0 / mode.frequency_hz;
        const Real Sa = interpolateSpectrum(input.spectrum, T);
        Eigen::VectorXd U = (Gamma * Sa / omega2) * mode.shape;
        Eigen::VectorXd F = Gamma * Sa * (Mfull * mode.shape);
        std::vector<NodalLoad> loads; loads.reserve(m.nodes.size());
        for (const auto& n : m.nodes) {
          auto d = dm.node_dofs(n.id);
          NodalLoad l; l.node = n.id;
          for (int k = 0; k < 6; ++k) l.values(k) = F(static_cast<Eigen::Index>(d[k]));
          loads.push_back(l);
        }
        AnalysisResults rec = LinearStaticSolver::recover(m, all, loads, U);

        ModalContribution mc;
        mc.period_s = T; mc.frequency_hz = mode.frequency_hz; mc.participation_factor = Gamma;
        mc.effective_mass = Gamma * Gamma;
        mc.mass_participation_ratio = out.total_mass > 0 ? mc.effective_mass / out.total_mass : 0;
        mc.results = rec;
        out.total_mass_participation_ratio += mc.mass_participation_ratio;
        out.modes.push_back(mc);

        for (const auto& nr : rec.nodes) { auto& ds = dispByMode[nr.node_id]; auto& rq = reacByMode[nr.node_id]; for (int k = 0; k < 6; ++k) { ds[k].push_back(nr.displacement[k]); rq[k].push_back(nr.reaction[k]); } }
        for (const auto& er : rec.elements) { auto& lfs = localForceByMode[er.element_id]; auto& fs = forceByMode[er.element_id]; for (int k = 0; k < 12; ++k) { lfs[k].push_back(er.local_end_force[k]); fs[k].push_back(er.global_end_force[k]); } }
      }

      // Cross-modal correlation matrix: identity for SRSS (every off-diagonal term is zero, so
      // the general formula below reduces exactly to a plain sum of squares), Der Kiureghian's
      // CQC coefficients otherwise -- see this file's header comment for the formula/reference.
      const std::size_t nModes = modal.modes.size();
      Eigen::MatrixXd rho = Eigen::MatrixXd::Identity(static_cast<Eigen::Index>(nModes), static_cast<Eigen::Index>(nModes));
      if (input.combination == ModalCombination::CQC) {
        const Real zeta = input.damping_ratio;
        if (!(std::isfinite(zeta) && zeta > 0)) throw std::invalid_argument("CQC requires a finite, positive damping_ratio");
        for (std::size_t i = 0; i < nModes; ++i)
          for (std::size_t j = 0; j < nModes; ++j)
            rho(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) = cqcCorrelation(modal.modes[i].angular_frequency_rad_s, modal.modes[j].angular_frequency_rad_s, zeta);
      }
      auto combine = [&](const std::vector<Real>& R) -> Real {
        Real sum = 0;
        for (std::size_t i = 0; i < nModes; ++i)
          for (std::size_t j = 0; j < nModes; ++j) sum += rho(static_cast<Eigen::Index>(i), static_cast<Eigen::Index>(j)) * R[i] * R[j];
        return std::sqrt(std::max(Real(0), sum)); // round-off on a near-zero sum must never produce NaN
      };

      for (const auto& n : m.nodes) { NodalPeakResponse p; p.node_id = n.id; for (int k = 0; k < 6; ++k) { p.displacement[k] = combine(dispByMode[n.id][k]); p.reaction[k] = combine(reacByMode[n.id][k]); } out.combined_nodes.push_back(p); }
      for (const auto& e : m.beams) { ElementPeakResponse p; p.element_id = e.id; for (int k = 0; k < 12; ++k) { p.local_end_force[k] = combine(localForceByMode[e.id][k]); p.global_end_force[k] = combine(forceByMode[e.id][k]); } out.combined_elements.push_back(p); }
      out.status = {true, "response spectrum analysis completed", SolverStatusCode::Success};
    } catch (const std::exception& e) { out.status = {false, e.what(), SolverStatusCode::InvalidModel}; }
    return out;
  }
};

} // namespace kopruq
