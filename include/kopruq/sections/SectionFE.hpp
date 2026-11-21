#pragma once
// 2D finite-element cross-section analysis (2026-11-20): Saint-Venant torsion constant J and
// Timoshenko shear areas of an arbitrary polygonal section, with any number of holes (multiply
// connected), on a mesh of linear (3-node) triangles. Used for sections whose J/shear areas have
// no closed form -- first MIDAS NX's PSC box sections, which MIDAS itself evaluates numerically
// on a section mesh (see docs/DEVELOPMENT_STATUS.md, 2026-11-20, model 22).
//
// Coordinates: the section's local (y, z) plane, y horizontal, z vertical (same as
// SectionGeometry.hpp: Point2.x() = y, Point2.y() = z). Everything is evaluated about the
// centroid of the meshed region.
//
// Torsion (warping-function formulation, valid for multiply connected sections without any
// extra hole condition): find w with
//     int grad(v) . grad(w) dA = int ( z dv/dy - y dv/dz ) dA      for all v   (pure Neumann)
//     J = Ip + int ( y dw/dz - z dw/dy ) dA,     Ip = int (y^2 + z^2) dA.
// A conforming linear FE solution over-estimates J and converges from above (energy bound).
//
// Shear areas (Pilkey, "Analysis and Design of Elastic Beams", 2002, shear functions; with
// x := y horizontal and "y" := z vertical, Ixx := int z^2, Iyy := int y^2, Ixy := int y z):
//     int grad(v).grad(Psi) dA = int [ nu/2 grad(v).d1 + 2(1+nu) v (Ixx y - Ixy z) ] dA
//     int grad(v).grad(Phi) dA = int [ nu/2 grad(v).d2 + 2(1+nu) v (Iyy z - Ixy y) ] dA
//     d1 = (Ixx r - Ixy q, Ixy r + Ixx q),  d2 = (Iyy q + Ixy r, -Iyy r + Ixy q),  r = y^2 - z^2, q = 2 y z
//     kappa_y = int |grad Psi - nu/2 d1|^2 dA,  kappa_z = int |grad Phi - nu/2 d2|^2 dA
//     Delta = 2(1+nu)(Ixx Iyy - Ixy^2),  As_y = Delta^2 / kappa_y (shear along y),
//                                        As_z = Delta^2 / kappa_z (shear along z)
// Checked: nu = 0 gives exactly 5/6*A for any rectangle and 6/7*A for a circle (the energy-
// consistent Timoshenko values). Loads are integrated with a 6-point degree-4 Dunavant rule
// (exact for the quadratic/quartic integrands on a linear triangle).
//
// Mesh (own implementation, no external mesher): boundary edges sampled at spacing <= h, plus a
// triangular lattice of interior points kept >= 0.55 h from every boundary edge; incremental
// Bowyer-Watson Delaunay triangulation; every boundary sub-segment is then checked to be a mesh
// edge and, if one is missing, it is split at its midpoint and the triangulation rebuilt
// (conforming Delaunay); finally triangles whose centroid lies outside the solid are removed.
// The meshed area must equal the exact polygon area (relative 1e-9) or the analysis throws.
#include "SectionGeometry.hpp"
#include <Eigen/Sparse>
#include <Eigen/SparseCholesky>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>
#include <vector>

namespace kopruq::sections {

struct SectionMesh {
  std::vector<Point2> nodes;
  std::vector<std::array<int, 3>> triangles; // counter-clockwise
};

struct SectionFEResult {
  Real area{}, J{}, Asy{}, Asz{}; // Asy: shear along local y (horizontal); Asz: along local z (vertical)
  Real Iyy{}, Izz{};              // from the mesh, about its centroid (cross-check of the exact polygon values)
  std::size_t nodes{}, triangles{};
};

namespace detail_fe {

inline Real orient(const Point2& a, const Point2& b, const Point2& c) {
  return (b.x() - a.x()) * (c.y() - a.y()) - (b.y() - a.y()) * (c.x() - a.x());
}
// > 0 when p is strictly inside the circumcircle of the counter-clockwise triangle (a, b, c).
inline Real inCircle(const Point2& a, const Point2& b, const Point2& c, const Point2& p) {
  const Real adx = a.x() - p.x(), ady = a.y() - p.y(), bdx = b.x() - p.x(), bdy = b.y() - p.y();
  const Real cdx = c.x() - p.x(), cdy = c.y() - p.y();
  return (adx * adx + ady * ady) * (bdx * cdy - cdx * bdy) - (bdx * bdx + bdy * bdy) * (adx * cdy - cdx * ady) +
         (cdx * cdx + cdy * cdy) * (adx * bdy - bdx * ady);
}
inline bool pointInContour(const Contour& c, const Point2& p) {
  bool in = false;
  for (std::size_t i = 0, n = c.size(); i < n; ++i) {
    const Point2& a = c[i];
    const Point2& b = c[(i + 1) % n];
    if ((a.y() > p.y()) != (b.y() > p.y())) {
      const Real x = a.x() + (p.y() - a.y()) * (b.x() - a.x()) / (b.y() - a.y());
      if (p.x() < x) in = !in;
    }
  }
  return in;
}
inline bool inSolid(const std::vector<Contour>& contours, const Point2& p) {
  if (!pointInContour(contours[0], p)) return false;
  for (std::size_t k = 1; k < contours.size(); ++k)
    if (pointInContour(contours[k], p)) return false;
  return true;
}
inline Real segmentDistance(const Point2& a, const Point2& b, const Point2& p) {
  const Point2 ab = b - a;
  const Real t = std::clamp((p - a).dot(ab) / ab.squaredNorm(), Real(0), Real(1));
  return (a + t * ab - p).norm();
}

// Incremental Bowyer-Watson Delaunay triangulation of `pts` (in insertion order).
inline std::vector<std::array<int, 3>> delaunay(const std::vector<Point2>& pts) {
  const int n = static_cast<int>(pts.size());
  if (n < 3) throw std::invalid_argument("SectionFE: too few mesh points");
  Real ymin = pts[0].x(), ymax = ymin, zmin = pts[0].y(), zmax = zmin;
  for (const auto& p : pts) { ymin = std::min(ymin, p.x()); ymax = std::max(ymax, p.x()); zmin = std::min(zmin, p.y()); zmax = std::max(zmax, p.y()); }
  const Real span = std::max(ymax - ymin, zmax - zmin), cy = (ymin + ymax) / 2, cz = (zmin + zmax) / 2;
  std::vector<Point2> P = pts;
  P.emplace_back(cy - 20 * span, cz - 10 * span);
  P.emplace_back(cy + 20 * span, cz - 10 * span);
  P.emplace_back(cy, cz + 20 * span);
  struct Tri { std::array<int, 3> v; std::array<int, 3> nb; bool alive; };
  std::vector<Tri> T;
  T.reserve(static_cast<std::size_t>(n) * 2 + 16);
  T.push_back({{n, n + 1, n + 2}, {-1, -1, -1}, true}); // nb[i]: neighbour across the edge opposite v[i]
  int last = 0;
  std::vector<int> cavity, stack;
  // inCavMark[c] == stamp <=> triangle c is in the current cavity (stamps avoid an O(#triangles) reset
  // per inserted point).
  std::vector<int> inCavMark, seenMark;
  int stamp = 0;
  for (int ip = 0; ip < n; ++ip) {
    const Point2& p = P[ip];
    // Locate a triangle containing p by walking from the last one created.
    int t = last;
    for (int guard = 0;; ++guard) {
      if (guard > 4 * (int)T.size() + 100) throw std::runtime_error("SectionFE: point location failed");
      const auto& tr = T[t];
      int next = -1;
      for (int e = 0; e < 3; ++e) {
        const Point2& a = P[tr.v[(e + 1) % 3]];
        const Point2& b = P[tr.v[(e + 2) % 3]];
        if (orient(a, b, p) < 0) { next = tr.nb[e]; break; }
      }
      if (next < 0) break;
      t = next;
    }
    // Cavity: triangles whose circumcircle strictly contains p, grown from t.
    if (inCavMark.size() < T.size()) { inCavMark.resize(T.size() * 2, 0); seenMark.resize(T.size() * 2, 0); }
    ++stamp;
    auto inCav = [&](int c) { return inCavMark[static_cast<std::size_t>(c)] == stamp; };
    cavity.clear(); stack.assign(1, t); inCavMark[static_cast<std::size_t>(t)] = stamp;
    while (!stack.empty()) {
      const int c = stack.back(); stack.pop_back(); cavity.push_back(c);
      for (int e = 0; e < 3; ++e) {
        const int nb = T[c].nb[e];
        if (nb < 0 || inCav(nb)) continue;
        const auto& q = T[nb].v;
        if (inCircle(P[q[0]], P[q[1]], P[q[2]], p) > 0) { inCavMark[static_cast<std::size_t>(nb)] = stamp; stack.push_back(nb); }
      }
    }
    // Keep the cavity star-shaped w.r.t. p (robustness against round-off): drop any cavity triangle
    // (other than t) owning a boundary edge that p does not see strictly from inside, then keep only
    // the part still connected to t.
    for (bool changed = true; changed;) {
      changed = false;
      for (int c : cavity) {
        if (!inCav(c) || c == t) continue;
        for (int e = 0; e < 3; ++e) {
          const int nb = T[c].nb[e];
          if (nb >= 0 && inCav(nb)) continue;
          if (orient(P[T[c].v[(e + 1) % 3]], P[T[c].v[(e + 2) % 3]], p) <= 0) { inCavMark[static_cast<std::size_t>(c)] = 0; changed = true; break; }
        }
      }
      if (changed) {
        std::vector<int> keep; stack.assign(1, t); seenMark[static_cast<std::size_t>(t)] = stamp;
        while (!stack.empty()) {
          const int c = stack.back(); stack.pop_back(); keep.push_back(c);
          for (int e = 0; e < 3; ++e) {
            const int nb = T[c].nb[e];
            if (nb >= 0 && inCav(nb) && seenMark[static_cast<std::size_t>(nb)] != stamp) { seenMark[static_cast<std::size_t>(nb)] = stamp; stack.push_back(nb); }
          }
        }
        for (int c : cavity) if (seenMark[static_cast<std::size_t>(c)] != stamp) inCavMark[static_cast<std::size_t>(c)] = 0;
        cavity = keep;
        ++stamp; // fresh stamp for a possible next re-collection; re-mark the kept cavity
        for (int c : cavity) inCavMark[static_cast<std::size_t>(c)] = stamp;
      }
    }
    // Boundary edges of the cavity -> new triangles (a, b, p).
    struct BEdge { int a, b, outside; };
    std::vector<BEdge> edges;
    for (int c : cavity)
      for (int e = 0; e < 3; ++e) {
        const int nb = T[c].nb[e];
        if (nb >= 0 && inCav(nb)) continue;
        edges.push_back({T[c].v[(e + 1) % 3], T[c].v[(e + 2) % 3], nb});
      }
    for (int c : cavity) T[c].alive = false;
    std::map<std::pair<int, int>, std::pair<int, int>> open; // directed edge (u,v) of a new triangle -> (tri, edge index)
    std::vector<int> created;
    for (const auto& be : edges) {
      const int id = static_cast<int>(T.size());
      T.push_back({{be.a, be.b, ip}, {-1, -1, -1}, true});
      created.push_back(id);
      T[id].nb[2] = be.outside; // edge (a,b) is opposite p (index 2)
      if (be.outside >= 0)
        for (int e = 0; e < 3; ++e) {
          const int u = T[be.outside].v[(e + 1) % 3], w = T[be.outside].v[(e + 2) % 3];
          if (u == be.b && w == be.a) T[be.outside].nb[e] = id;
        }
      // edge (b, p) is opposite a (index 0); edge (p, a) is opposite b (index 1)
      open[{be.b, ip}] = {id, 0};
      open[{ip, be.a}] = {id, 1};
    }
    for (const auto& [key, val] : open) {
      auto it = open.find({key.second, key.first});
      if (it != open.end()) T[val.first].nb[val.second] = it->second.first;
    }
    last = created.empty() ? 0 : created.back();
  }
  std::vector<std::array<int, 3>> out;
  for (const auto& tr : T)
    if (tr.alive && tr.v[0] < n && tr.v[1] < n && tr.v[2] < n) out.push_back(tr.v);
  return out;
}

} // namespace detail_fe

// Mesh the solid region of `contours` (contours[0] = outer boundary, the rest = holes; any
// winding) with target spacing h.
inline SectionMesh meshPolygonSection(const std::vector<Contour>& contours, Real h) {
  using namespace detail_fe;
  if (contours.empty() || contours[0].size() < 3) throw std::invalid_argument("SectionFE: an outer contour with >= 3 vertices is required");
  if (!(h > 0)) throw std::invalid_argument("SectionFE: mesh size must be positive");
  // Boundary samples, kept per contour as closed point loops.
  std::vector<std::vector<Point2>> loops;
  for (const auto& c : contours) {
    std::vector<Point2> loop;
    for (std::size_t i = 0, m = c.size(); i < m; ++i) {
      const Point2& a = c[i];
      const Point2& b = c[(i + 1) % m];
      const int k = std::max(1, static_cast<int>(std::ceil((b - a).norm() / h - 1e-9)));
      for (int j = 0; j < k; ++j) loop.push_back(a + (b - a) * (Real(j) / k));
    }
    loops.push_back(loop);
  }
  Real ymin = contours[0][0].x(), ymax = ymin, zmin = contours[0][0].y(), zmax = zmin;
  for (const auto& p : contours[0]) { ymin = std::min(ymin, p.x()); ymax = std::max(ymax, p.x()); zmin = std::min(zmin, p.y()); zmax = std::max(zmax, p.y()); }
  // Interior triangular lattice, away from the boundary.
  std::vector<Point2> interior;
  const Real dz = h * std::sqrt(Real(3)) / 2;
  for (int row = 0; zmin + (row + 0.5) * dz < zmax; ++row) {
    const Real z = zmin + (row + 0.5) * dz;
    for (Real y = ymin + (row % 2 ? h : h / 2); y < ymax; y += h) {
      const Point2 p(y, z);
      if (!inSolid(contours, p)) continue;
      Real dmin = std::numeric_limits<Real>::max();
      for (const auto& c : contours)
        for (std::size_t i = 0, m = c.size(); i < m && dmin >= 0.55 * h; ++i) dmin = std::min(dmin, segmentDistance(c[i], c[(i + 1) % m], p));
      if (dmin >= 0.55 * h) interior.push_back(p);
    }
  }
  for (int pass = 0; pass < 30; ++pass) {
    std::vector<Point2> pts;
    std::vector<std::pair<int, int>> segments;
    for (const auto& loop : loops) {
      const int base = static_cast<int>(pts.size()), m = static_cast<int>(loop.size());
      for (const auto& p : loop) pts.push_back(p);
      for (int i = 0; i < m; ++i) segments.emplace_back(base + i, base + (i + 1) % m);
    }
    const int nb = static_cast<int>(pts.size());
    pts.insert(pts.end(), interior.begin(), interior.end());
    // Spatially coherent insertion order (rows of width h) for a short point-location walk.
    std::vector<int> order(pts.size());
    for (std::size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
      const long ra = std::lround(std::floor((pts[a].y() - zmin) / (4 * h))), rb = std::lround(std::floor((pts[b].y() - zmin) / (4 * h)));
      if (ra != rb) return ra < rb;
      return (ra % 2 == 0) ? pts[a].x() < pts[b].x() : pts[a].x() > pts[b].x();
    });
    std::vector<Point2> sorted(pts.size());
    std::vector<int> rank(pts.size());
    for (std::size_t i = 0; i < order.size(); ++i) { sorted[i] = pts[order[i]]; rank[order[i]] = static_cast<int>(i); }
    auto tris = delaunay(sorted);
    // Boundary recovery check.
    std::map<std::pair<int, int>, int> edgeSet;
    for (const auto& t : tris)
      for (int e = 0; e < 3; ++e) { const int a = t[e], b = t[(e + 1) % 3]; edgeSet[{std::min(a, b), std::max(a, b)}] = 1; }
    std::vector<std::pair<int, int>> missing;
    for (const auto& [a, b] : segments) {
      const int ra = rank[a], rb = rank[b];
      if (!edgeSet.count({std::min(ra, rb), std::max(ra, rb)})) missing.emplace_back(a, b);
    }
    if (missing.empty()) {
      SectionMesh mesh;
      mesh.nodes = sorted;
      for (const auto& t : tris) {
        const Point2 c = (sorted[t[0]] + sorted[t[1]] + sorted[t[2]]) / 3;
        if (inSolid(contours, c)) mesh.triangles.push_back(t);
      }
      (void)nb;
      return mesh;
    }
    // Split every missing boundary sub-segment at its midpoint and rebuild.
    std::vector<std::vector<Point2>> next;
    int offset = 0;
    for (const auto& loop : loops) {
      const int m = static_cast<int>(loop.size());
      std::vector<Point2> nl;
      for (int i = 0; i < m; ++i) {
        nl.push_back(loop[i]);
        const int a = offset + i, b = offset + (i + 1) % m;
        if (std::find(missing.begin(), missing.end(), std::make_pair(a, b)) != missing.end()) nl.push_back((loop[i] + loop[(i + 1) % m]) / 2);
      }
      next.push_back(nl);
      offset += m;
    }
    loops = next;
  }
  throw std::runtime_error("SectionFE: boundary recovery did not converge");
}

// Torsion constant and shear areas of the polygonal section `contours` (contours[0] outer, rest
// holes). h: target element size; nu: Poisson's ratio in the shear functions (nu = 0 gives the
// classical energy-consistent Timoshenko shear areas, e.g. exactly 5/6 A for a rectangle).
// Project standard mesh size for section properties computed on a section mesh (J, Asy, Asz of PSC
// and any other meshed section): h = 0.01 m (user decision, 2026-11-21). Model 22/23 box girder:
// 123k nodes, about 1.5 s; J/Asy/Asz within 0.01% of the h -> 0 limit.
inline constexpr Real kStandardSectionMeshSize = 0.01;

inline SectionFEResult analyzeSectionFE(const std::vector<Contour>& contours, Real h, Real nu = 0) {
  const SectionMesh mesh = meshPolygonSection(contours, h);
  const int n = static_cast<int>(mesh.nodes.size());
  // Centroid of the meshed region.
  Real A = 0, Sy = 0, Sz = 0;
  for (const auto& t : mesh.triangles) {
    const Point2 &a = mesh.nodes[t[0]], &b = mesh.nodes[t[1]], &c = mesh.nodes[t[2]];
    const Real ar = detail_fe::orient(a, b, c) / 2;
    if (!(ar > 0)) throw std::runtime_error("SectionFE: degenerate or inverted triangle");
    A += ar; Sy += ar * (a.x() + b.x() + c.x()) / 3; Sz += ar * (a.y() + b.y() + c.y()) / 3;
  }
  const Real yc = Sy / A, zc = Sz / A;
  {
    // The mesh must reproduce the exact polygon area (proof that the boundary is conforming).
    std::vector<Contour> oriented = contours;
    for (std::size_t k = 0; k < oriented.size(); ++k) {
      Real s = 0;
      for (std::size_t i = 0, m = oriented[k].size(); i < m; ++i) s += oriented[k][i].x() * oriented[k][(i + 1) % m].y() - oriented[k][(i + 1) % m].x() * oriented[k][i].y();
      if ((k == 0) != (s > 0)) std::reverse(oriented[k].begin(), oriented[k].end());
    }
    const Real exactA = computePolygonSection(oriented).area;
    if (std::abs(A - exactA) > 1e-9 * exactA) throw std::runtime_error("SectionFE: meshed area does not match the polygon area");
  }
  // Dunavant degree-4, 6-point rule (area coordinates, weights sum to 1).
  const Real a1 = 0.445948490915965, b1 = 0.108103018168070, w1 = 0.223381589678011;
  const Real a2 = 0.091576213509771, b2 = 0.816847572980459, w2 = 0.109951743655322;
  const std::array<std::array<Real, 4>, 6> QP{{{a1, a1, b1, w1}, {a1, b1, a1, w1}, {b1, a1, a1, w1},
                                               {a2, a2, b2, w2}, {a2, b2, a2, w2}, {b2, a2, a2, w2}}};
  struct Elem { std::array<int, 3> v; Real area; Eigen::Vector3d by, bz; std::array<Eigen::Vector3d, 6> N; std::array<Real, 6> y, z, w; };
  std::vector<Elem> E;
  E.reserve(mesh.triangles.size());
  Real Iyy = 0, Izz = 0, Iyz = 0, Ip = 0; // Iyy = int z^2 (this project's Iy), Izz = int y^2
  for (const auto& t : mesh.triangles) {
    Elem e; e.v = t;
    Eigen::Vector3d Y, Z;
    for (int i = 0; i < 3; ++i) { Y(i) = mesh.nodes[t[i]].x() - yc; Z(i) = mesh.nodes[t[i]].y() - zc; }
    e.area = ((Y(1) - Y(0)) * (Z(2) - Z(0)) - (Y(2) - Y(0)) * (Z(1) - Z(0))) / 2;
    e.by = Eigen::Vector3d(Z(1) - Z(2), Z(2) - Z(0), Z(0) - Z(1)) / (2 * e.area); // dN/dy
    e.bz = Eigen::Vector3d(Y(2) - Y(1), Y(0) - Y(2), Y(1) - Y(0)) / (2 * e.area); // dN/dz
    for (int q = 0; q < 6; ++q) {
      e.N[q] = Eigen::Vector3d(QP[q][0], QP[q][1], QP[q][2]);
      e.y[q] = e.N[q].dot(Y); e.z[q] = e.N[q].dot(Z); e.w[q] = QP[q][3] * e.area;
      Iyy += e.w[q] * e.z[q] * e.z[q]; Izz += e.w[q] * e.y[q] * e.y[q]; Iyz += e.w[q] * e.y[q] * e.z[q];
    }
    E.push_back(e);
  }
  Ip = Iyy + Izz;
  // Stiffness (Laplacian) and the three load vectors.
  std::vector<Eigen::Triplet<Real>> trip;
  trip.reserve(E.size() * 9);
  Eigen::VectorXd fw = Eigen::VectorXd::Zero(n), fpsi = Eigen::VectorXd::Zero(n), fphi = Eigen::VectorXd::Zero(n);
  // Pilkey names: Ixx = int (vertical)^2 = Iyy here, Iyy_P = int (horizontal)^2 = Izz here, Ixy = Iyz.
  const Real IxxP = Iyy, IyyP = Izz, IxyP = Iyz;
  for (const auto& e : E) {
    const Eigen::Matrix3d K = e.area * (e.by * e.by.transpose() + e.bz * e.bz.transpose());
    for (int i = 0; i < 3; ++i)
      for (int j = 0; j < 3; ++j) trip.emplace_back(e.v[i], e.v[j], K(i, j));
    for (int q = 0; q < 6; ++q) {
      const Real y = e.y[q], z = e.z[q], w = e.w[q], r = y * y - z * z, s = 2 * y * z;
      const Eigen::Vector3d gw = w * (z * e.by - y * e.bz);
      const Real d1y = IxxP * r - IxyP * s, d1z = IxyP * r + IxxP * s;
      const Real d2y = IyyP * s + IxyP * r, d2z = -IyyP * r + IxyP * s;
      const Eigen::Vector3d gpsi = w * (nu / 2 * (e.by * d1y + e.bz * d1z) + 2 * (1 + nu) * e.N[q] * (IxxP * y - IxyP * z));
      const Eigen::Vector3d gphi = w * (nu / 2 * (e.by * d2y + e.bz * d2z) + 2 * (1 + nu) * e.N[q] * (IyyP * z - IxyP * y));
      for (int i = 0; i < 3; ++i) { fw(e.v[i]) += gw(i); fpsi(e.v[i]) += gpsi(i); fphi(e.v[i]) += gphi(i); }
    }
  }
  // Pure Neumann problems: pin node 0 (the solutions are defined up to a constant, which no
  // result below depends on -- only gradients enter J and the shear coefficients).
  for (auto& tp : trip)
    if (tp.row() == 0 || tp.col() == 0) tp = Eigen::Triplet<Real>(tp.row(), tp.col(), 0);
  trip.emplace_back(0, 0, 1);
  fw(0) = fpsi(0) = fphi(0) = 0;
  Eigen::SparseMatrix<Real> Kg(n, n);
  Kg.setFromTriplets(trip.begin(), trip.end());
  Eigen::SimplicialLDLT<Eigen::SparseMatrix<Real>> solver(Kg);
  if (solver.info() != Eigen::Success) throw std::runtime_error("SectionFE: factorization failed");
  const Eigen::VectorXd w = solver.solve(fw), psi = solver.solve(fpsi), phi = solver.solve(fphi);
  if (solver.info() != Eigen::Success) throw std::runtime_error("SectionFE: solve failed");
  Real J = Ip, ky = 0, kz = 0;
  for (const auto& e : E) {
    Eigen::Vector3d we, pe, fe;
    for (int i = 0; i < 3; ++i) { we(i) = w(e.v[i]); pe(i) = psi(e.v[i]); fe(i) = phi(e.v[i]); }
    const Real wy = e.by.dot(we), wz = e.bz.dot(we), py = e.by.dot(pe), pz = e.bz.dot(pe), fy = e.by.dot(fe), fz = e.bz.dot(fe);
    for (int q = 0; q < 6; ++q) {
      const Real y = e.y[q], z = e.z[q], wq = e.w[q], r = y * y - z * z, s = 2 * y * z;
      J += wq * (y * wz - z * wy);
      const Real d1y = IxxP * r - IxyP * s, d1z = IxyP * r + IxxP * s;
      const Real d2y = IyyP * s + IxyP * r, d2z = -IyyP * r + IxyP * s;
      ky += wq * ((py - nu / 2 * d1y) * (py - nu / 2 * d1y) + (pz - nu / 2 * d1z) * (pz - nu / 2 * d1z));
      kz += wq * ((fy - nu / 2 * d2y) * (fy - nu / 2 * d2y) + (fz - nu / 2 * d2z) * (fz - nu / 2 * d2z));
    }
  }
  const Real Delta = 2 * (1 + nu) * (IxxP * IyyP - IxyP * IxyP);
  SectionFEResult res;
  res.area = A; res.J = J; res.Asy = Delta * Delta / ky; res.Asz = Delta * Delta / kz;
  res.Iyy = Iyy; res.Izz = Izz; res.nodes = static_cast<std::size_t>(n); res.triangles = mesh.triangles.size();
  return res;
}

} // namespace kopruq::sections
