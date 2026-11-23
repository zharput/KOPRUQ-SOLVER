#pragma once
// Internal forces along Beam3D members ("stations"), from free-body equilibrium (2026-11-23,
// design: ui/docs/station-results-design.md). Pure post-processing of values the solver already has:
// the recovered local end forces (ElementResults::local_end_force) and the element's own loads.
//
// Convention (solver's own local axes, no MIDAS sign flip): at x along the member axis the six values
// are the force N, Vy, Vz and the moment T, My, Mz that the part of the member beyond x exerts on the part
// before x, i.e. the resultant acting on the +x face of the piece [0, x] (N > 0 tension; at x = L this is the
// node-2 end force). Free body [0,x]:
//   R(x) = -f1 - sum F_load(s<x)                      (force)
//   M(x) = -m1 + x e_x x f1 - sum M_load(about x)     (moment about the cut point on the member axis)
// with f1, m1 the node-1 end force/moment. Loads exactly at a station: "before" = excluded, "after" = included
// (a point load inside the member gives two stations with the same x). Loads at x=0 count for x=0, loads at
// x=L do not count for x=L (the station is just inside the member).
// Thermal loads act only through the equivalent nodal loads (no internal distributed term), so they need
// nothing here. Equilibrium does not depend on the member kinematics (Euler-Bernoulli / Timoshenko), nor on
// releases (a released end force is zero).
#include "../solvers/LinearStaticSolver.hpp"
#include <tuple>
#include <array>
#include <cmath>
#include <vector>

namespace kopruq {

struct StationRow { Real x{}; char side{'s'}; std::array<Real, 6> f{}; };  // side: 's' single, 'b' before, 'a' after a point load
struct ElementStations {
  Id element{}; Real length{};
  std::vector<StationRow> rows;
  // Closure: |R(L; all loads) - f2| and |M(L; all loads) - m2| -- zero when the recovered end forces and the loads are in equilibrium.
  Real closure_force{}, closure_moment{};
};

namespace detail {
struct LocalLoad {            // one load acting on a member, in member-local components
  bool point{}; bool moment{};
  Eigen::Vector3d v{0, 0, 0}; // direction (local components)
  Real a{}, b{}, w1{}, w2{};  // distributed: segment [a,b] and intensities (metres)
  Real p{}, value{};          // point: position and value
};

// Cumulative resultants of the loads on the piece [0,x] about the cut at x. `after`: include loads exactly at x.
inline void cumulativeLoads(const std::vector<LocalLoad>& ls, Real x, bool after, Real tol, Eigen::Vector3d& F, Eigen::Vector3d& M) {
  F.setZero(); M.setZero(); const Eigen::Vector3d ex(1, 0, 0);
  for (const auto& l : ls) {
    if (l.point) {
      const bool in = l.p < x - tol || (std::abs(l.p - x) <= tol && after);
      if (!in) continue;
      if (l.moment) M += l.v * l.value; else { F += l.v * l.value; M += (l.p - x) * ex.cross(l.v) * l.value; }
    } else {
      const Real c = std::min(x, l.b), t = c - l.a; if (t <= 0 || l.b <= l.a) continue;
      const Real k = (l.w2 - l.w1) / (l.b - l.a);
      const Real W = l.w1 * t + k * t * t / 2;                                   // integral of w ds
      if (l.moment) { M += l.v * W; continue; }
      const Real m0 = l.a * W + l.w1 * t * t / 2 + k * t * t * t / 3 - x * W;    // integral of w (s - x) ds
      F += l.v * W; M += m0 * ex.cross(l.v);
    }
  }
}
} // namespace detail

/** Stations of every Beam3D element for one (already solved) load case; `lc` is the case or the combined load case of a combination. */
inline std::vector<ElementStations> computeElementStations(const Model& m, const LoadCase& lc, const AnalysisResults& res, int uniformStations = 11) {
  auto nodes = detail::index_by_id(m.nodes); auto mats = detail::index_by_id(m.materials); auto secs = detail::index_by_id(m.sections);
  std::unordered_map<Id, const ElementResults*> er; for (const auto& e : res.elements) er[e.element_id] = &e;
  std::unordered_map<Id, std::vector<detail::LocalLoad>> loads;
  std::unordered_map<Id, std::tuple<Eigen::Matrix3d, Real>> frames;
  for (const auto& e : m.beams) {
    const Node* n1 = detail::find_by_id(nodes, e.node1); const Node* n2 = detail::find_by_id(nodes, e.node2);
    Beam3DGeometry g{Eigen::Vector3d(n1->x, n1->y, n1->z) + e.offset1, Eigen::Vector3d(n2->x, n2->y, n2->z) + e.offset2, e.reference_y};
    const auto ax = Beam3D::axes(g); Eigen::Matrix3d R; R.row(0) = ax.x.transpose(); R.row(1) = ax.y.transpose(); R.row(2) = ax.z.transpose();
    frames[e.id] = {R, (g.node2 - g.node1).norm()};
  }
  auto addMember = [&](const MemberLoad& ld) {
    const auto& [R, L] = frames.at(ld.element);
    detail::LocalLoad l; l.point = false; l.moment = ld.kind == MemberLoadKind::Moment;
    Eigen::Vector3d e = Eigen::Vector3d::Zero(); e(ld.direction) = 1; l.v = ld.axes == LoadAxes::Local ? e : (R * e).eval();
    Real ratio = 1; if (ld.projected) { const Real c = R(0, ld.direction); ratio = std::sqrt(std::max(Real(0), 1 - c * c)); }
    l.a = ld.start * L; l.b = ld.end * L; l.w1 = ld.w1 * ratio; l.w2 = ld.w2 * ratio; loads[ld.element].push_back(l);
  };
  for (const auto& ld : lc.member) addMember(ld);
  if (std::isfinite(lc.self_weight) && lc.self_weight != 0)
    for (const auto& e : m.beams) {
      const Material* mat = detail::find_by_id(mats, e.material); const Section* sec = detail::find_by_id(secs, e.section);
      const Real w = -mat->unit_weight * sec->area * lc.self_weight; if (w != 0) addMember(MemberLoad{e.id, LoadAxes::Global, 2, w, w, 0, 1});
    }
  for (const auto& ld : lc.point) {
    const auto& [R, L] = frames.at(ld.element);
    detail::LocalLoad l; l.point = true; l.moment = ld.kind == MemberLoadKind::Moment; l.p = ld.position * L; l.value = ld.value;
    Eigen::Vector3d e = Eigen::Vector3d::Zero(); e(ld.direction) = 1; l.v = ld.axes == LoadAxes::Local ? e : (R * e).eval(); loads[ld.element].push_back(l);
  }
  std::vector<ElementStations> out; out.reserve(m.beams.size());
  const int n = std::max(2, uniformStations);
  for (const auto& e : m.beams) {
    const auto it = er.find(e.id); if (it == er.end()) continue;
    const Real L = std::get<1>(frames.at(e.id)); const auto& fl = it->second->local_end_force;
    const Eigen::Vector3d f1(fl[0], fl[1], fl[2]), m1(fl[3], fl[4], fl[5]), f2(fl[6], fl[7], fl[8]), m2(fl[9], fl[10], fl[11]);
    const auto& ls = loads[e.id]; const Real tol = 1e-9 * L; const Eigen::Vector3d ex(1, 0, 0);
    auto eval = [&](Real x, bool after) {
      Eigen::Vector3d F, M; detail::cumulativeLoads(ls, x, after, tol, F, M);
      return std::pair<Eigen::Vector3d, Eigen::Vector3d>{(-f1 - F).eval(), (-m1 + x * ex.cross(f1) - M).eval()};
    };
    // station positions: uniform, load segment ends, point loads (point loads strictly inside get a before/after pair)
    std::vector<std::pair<Real, bool>> xs;   // (x, jump)
    for (int i = 0; i < n; ++i) xs.push_back({L * i / (n - 1), false});
    for (const auto& l : ls) {
      if (l.point) { if (l.p > tol && l.p < L - tol) xs.push_back({l.p, true}); else xs.push_back({std::min(std::max(l.p, Real(0)), L), false}); }
      else { xs.push_back({l.a, false}); xs.push_back({l.b, false}); }
    }
    std::sort(xs.begin(), xs.end(), [](const auto& p, const auto& q) { return p.first < q.first; });
    std::vector<std::pair<Real, bool>> u;
    for (const auto& p : xs) { if (!u.empty() && std::abs(p.first - u.back().first) <= tol) u.back().second = u.back().second || p.second; else u.push_back(p); }
    ElementStations es; es.element = e.id; es.length = L;
    auto row = [&](Real x, bool after, char side) {
      const auto [R, M] = eval(x, after); es.rows.push_back({x, side, {R(0), R(1), R(2), M(0), M(1), M(2)}});
    };
    for (const auto& [x, jump] : u) {
      if (jump) { row(x, false, 'b'); row(x, true, 'a'); }
      else row(x, x <= tol, 's');            // x = 0: loads at the start count; x = L: they do not
    }
    { Eigen::Vector3d F, M; detail::cumulativeLoads(ls, L, true, tol, F, M);   // closure with every load, including those at L
      const Eigen::Vector3d R = -f1 - F, Mm = -m1 + L * ex.cross(f1) - M; es.closure_force = (R - f2).norm(); es.closure_moment = (Mm - m2).norm(); }
    out.push_back(std::move(es));
  }
  return out;
}

} // namespace kopruq
