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
// - Modal combination: SRSS (square root of the sum of squares) over every
//   displacement/reaction/end-force component independently. SRSS-combined
//   values are peak *estimates*: they are not a single equilibrium state and
//   generally do not satisfy sum(F)=0 themselves -- that is expected, not a
//   defect. CQC (for closely-spaced modes) is not implemented; see
//   docs/DEVELOPMENT_STATUS.md.
#include "ModalSolver.hpp"
#include <algorithm>

namespace kopruq {

struct ResponseSpectrumPoint { Real period_s{}, spectral_acceleration{}; }; // period [s], Sa [m/s^2]
struct ResponseSpectrumInput { std::vector<ResponseSpectrumPoint> spectrum; int direction{}; int num_modes{6}; };

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

struct ModalContribution { Real period_s{}, frequency_hz{}, participation_factor{}, effective_mass{}, mass_participation_ratio{}; AnalysisResults results; };
struct NodalPeakResponse { Id node_id{}; DofVector displacement{}, reaction{}; };
struct ElementPeakResponse { Id element_id{}; std::array<Real,12> global_end_force{}; };
struct ResponseSpectrumResults {
  SolverStatus status;
  std::vector<ModalContribution> modes;
  std::vector<NodalPeakResponse> combined_nodes;
  std::vector<ElementPeakResponse> combined_elements;
  Real total_mass{}, total_mass_participation_ratio{};
};

class ResponseSpectrumSolver { public:
  static ResponseSpectrumResults solve(const Model& m, const Restraints& rs, const ResponseSpectrumInput& input) {
    ResponseSpectrumResults out;
    try {
      if (input.direction < 0 || input.direction > 2) throw std::invalid_argument("direction must be 0 (x), 1 (y) or 2 (z)");
      auto modal = ModalSolver::solve(m, rs, input.num_modes);
      if (!modal.status.success) { out.status = modal.status; return out; }

      DofManager dm(m);
      Eigen::SparseMatrix<Real> Mfull = MatrixAssembler::assembleMass(m);

      auto all = rs; for (const auto& n : m.nodes) if (!all.count(n.id)) all[n.id] = {};
      auto free = MatrixAssembler::classify(dm, all).free;

      // iota: 1 at every FREE translational DOF matching `direction` (DOF index mod 6:
      // 0=UX, 1=UY, 2=UZ), 0 everywhere else, including restrained DOFs -- see the
      // formulation note above.
      Eigen::VectorXd iota = Eigen::VectorXd::Zero(static_cast<Eigen::Index>(dm.dof_count()));
      for (auto idx : free) if (idx % 6 == static_cast<std::size_t>(input.direction)) iota(static_cast<Eigen::Index>(idx)) = 1.0;
      Eigen::VectorXd Miota = Mfull * iota;
      out.total_mass = iota.dot(Miota);

      std::map<Id,std::array<Real,6>> dispSq, reacSq;
      std::map<Id,std::array<Real,12>> forceSq;
      for (const auto& n : m.nodes) { dispSq[n.id] = {}; reacSq[n.id] = {}; }
      for (const auto& e : m.beams) forceSq[e.id] = {};

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

        for (const auto& nr : rec.nodes) { auto& ds = dispSq[nr.node_id]; auto& rq = reacSq[nr.node_id]; for (int k = 0; k < 6; ++k) { ds[k] += nr.displacement[k] * nr.displacement[k]; rq[k] += nr.reaction[k] * nr.reaction[k]; } }
        for (const auto& er : rec.elements) { auto& fs = forceSq[er.element_id]; for (int k = 0; k < 12; ++k) fs[k] += er.global_end_force[k] * er.global_end_force[k]; }
      }
      for (const auto& n : m.nodes) { NodalPeakResponse p; p.node_id = n.id; for (int k = 0; k < 6; ++k) { p.displacement[k] = std::sqrt(dispSq[n.id][k]); p.reaction[k] = std::sqrt(reacSq[n.id][k]); } out.combined_nodes.push_back(p); }
      for (const auto& e : m.beams) { ElementPeakResponse p; p.element_id = e.id; for (int k = 0; k < 12; ++k) p.global_end_force[k] = std::sqrt(forceSq[e.id][k]); out.combined_elements.push_back(p); }
      out.status = {true, "response spectrum analysis completed", SolverStatusCode::Success};
    } catch (const std::exception& e) { out.status = {false, e.what(), SolverStatusCode::InvalidModel}; }
    return out;
  }
};

} // namespace kopruq
