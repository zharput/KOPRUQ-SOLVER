#pragma once
// Validates a Model, optional restraints, load cases and combinations built
// directly through the C++ API -- the same semantic checks the JSON reader
// applies (kopruq/io/ModelJson.hpp), with the same "report every issue, not
// just the first" behaviour, but addressed by index/id instead of JSON path,
// since there is no JSON document here. The two validators are intentionally
// separate: the JSON one also does type coercion from untyped JSON, which
// does not apply to already-typed C++ structs.
//
// This is purely additive: solveCases() and recover() are unchanged and
// still only report the first problem (via whatever exception the
// assembly/element code happens to throw). Call validateModel() beforehand
// for a complete, structured report.
#include "Model.hpp"
#include "../elements/Beam3D.hpp"
#include "../elements/Releases.hpp"
#include "../elements/SpringLink.hpp"
#include "../loads/Loads.hpp"
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace kopruq {

struct ModelIssue { std::string path, message; };

class ModelValidationError : public std::runtime_error {
 public:
  std::vector<ModelIssue> issues;
  explicit ModelValidationError(std::vector<ModelIssue> list) : std::runtime_error(format(list)), issues(std::move(list)) {}
  static std::string format(const std::vector<ModelIssue>& list) {
    std::string s = "invalid model (" + std::to_string(list.size()) + " issue" + (list.size() == 1 ? "" : "s") + ")";
    for (const auto& i : list) s += "\n  " + i.path + ": " + i.message;
    return s;
  }
};

namespace detail {

class ModelValidator {
 public:
  std::vector<ModelIssue> issues;

  void run(const Model& m, const Restraints& rs, const std::vector<LoadCase>& cases, const std::vector<LoadCombination>& combos) {
    nodes(m);
    materials(m);
    sections(m);
    beams(m);
    springs(m);
    restraints(m, rs);
    for (std::size_t i = 0; i < cases.size(); ++i) loadCase(m, cases[i], "load_cases[" + std::to_string(i) + "]");
    combinations(cases, combos);
  }

 private:
  std::set<Id> nodeIds_, materialIds_, sectionIds_, beamIds_;
  std::map<Id, Eigen::Vector3d> coords_;
  std::map<Id, const Material*> materials_;
  std::map<Id, const Section*> sections_;

  void issue(std::string path, std::string message) { issues.push_back({std::move(path), std::move(message)}); }
  static bool finite(const Eigen::Vector3d& v) { return v.allFinite(); }

  void nodes(const Model& m) {
    if (m.nodes.empty()) issue("nodes", "model must contain at least one node");
    for (std::size_t i = 0; i < m.nodes.size(); ++i) {
      const auto& n = m.nodes[i];
      const std::string p = "nodes[" + std::to_string(i) + "]";
      if (n.id == 0) { issue(p + ".id", "must be nonzero"); continue; }
      if (!nodeIds_.insert(n.id).second) { issue(p + ".id", "duplicate node id " + std::to_string(n.id)); continue; }
      if (!std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z)) issue(p, "coordinates must be finite");
      else coords_[n.id] = Eigen::Vector3d(n.x, n.y, n.z);
    }
  }
  void materials(const Model& m) {
    for (std::size_t i = 0; i < m.materials.size(); ++i) {
      const auto& mat = m.materials[i];
      const std::string p = "materials[" + std::to_string(i) + "]";
      if (mat.id == 0) { issue(p + ".id", "must be nonzero"); continue; }
      if (!materialIds_.insert(mat.id).second) { issue(p + ".id", "duplicate material id " + std::to_string(mat.id)); continue; }
      if (!std::isfinite(mat.elastic_modulus) || mat.elastic_modulus <= 0) issue(p + ".elastic_modulus", "must be finite and greater than 0");
      if (!std::isfinite(mat.poisson_ratio) || !(mat.poisson_ratio > -1 && mat.poisson_ratio < 0.5)) issue(p + ".poisson_ratio", "must satisfy -1 < poisson_ratio < 0.5");
      if (!std::isfinite(mat.unit_weight) || mat.unit_weight < 0) issue(p + ".unit_weight", "must be finite and >= 0");
      materials_[mat.id] = &mat;
    }
  }
  void sections(const Model& m) {
    for (std::size_t i = 0; i < m.sections.size(); ++i) {
      const auto& sec = m.sections[i];
      const std::string p = "sections[" + std::to_string(i) + "]";
      if (sec.id == 0) { issue(p + ".id", "must be nonzero"); continue; }
      if (!sectionIds_.insert(sec.id).second) { issue(p + ".id", "duplicate section id " + std::to_string(sec.id)); continue; }
      for (auto [name, v] : {std::pair{"area", sec.area}, {"Iy", sec.Iy}, {"Iz", sec.Iz}, {"J", sec.J}})
        if (!std::isfinite(v) || v <= 0) issue(p + "." + name, "must be finite and greater than 0");
      sections_[sec.id] = &sec;
    }
  }
  void beams(const Model& m) {
    for (std::size_t i = 0; i < m.beams.size(); ++i) {
      const auto& e = m.beams[i];
      const std::string p = "beams[" + std::to_string(i) + "]";
      if (e.id == 0) { issue(p + ".id", "must be nonzero"); continue; }
      if (!beamIds_.insert(e.id).second) { issue(p + ".id", "duplicate beam id " + std::to_string(e.id)); continue; }
      bool valid = true;
      if (!coords_.count(e.node1)) { issue(p + ".node1", "references unknown node " + std::to_string(e.node1)); valid = false; }
      if (!coords_.count(e.node2)) { issue(p + ".node2", "references unknown node " + std::to_string(e.node2)); valid = false; }
      if (valid && e.node1 == e.node2) { issue(p, "node1 and node2 must be different"); valid = false; }
      const Material* mat = nullptr; const Section* sec = nullptr;
      if (!materialIds_.count(e.material)) { issue(p + ".material", "references unknown material " + std::to_string(e.material)); valid = false; }
      else mat = materials_.at(e.material);
      if (!sectionIds_.count(e.section)) { issue(p + ".section", "references unknown section " + std::to_string(e.section)); valid = false; }
      else sec = sections_.at(e.section);
      if (!finite(e.reference_y)) { issue(p + ".reference_y", "must be finite"); valid = false; }
      if (!finite(e.offset1)) { issue(p + ".offset1", "must be finite"); valid = false; }
      if (!finite(e.offset2)) { issue(p + ".offset2", "must be finite"); valid = false; }
      if (!valid) continue;
      Beam3DGeometry g{coords_.at(e.node1) + e.offset1, coords_.at(e.node2) + e.offset2, e.reference_y};
      try { Beam3D::axes(g); }
      catch (const std::exception& ex) { issue(p, std::string("invalid geometry: ") + ex.what()); continue; }
      if (anyReleased(e.releases)) {
        Beam3DProperties bp{mat->elastic_modulus, mat->elastic_modulus / (2 * (1 + mat->poisson_ratio)), sec->area, sec->Iy, sec->Iz, sec->J};
        try { condensedStiffness(Beam3D::localStiffness((g.node2 - g.node1).norm(), bp), e.releases); }
        catch (const std::exception& ex) { issue(p + ".releases", ex.what()); }
      }
    }
  }
  void springs(const Model& m) {
    std::set<Id> springIds;
    for (std::size_t i = 0; i < m.springs.size(); ++i) {
      const auto& e = m.springs[i];
      const std::string p = "springs[" + std::to_string(i) + "]";
      if (e.id == 0) { issue(p + ".id", "must be nonzero"); continue; }
      if (!springIds.insert(e.id).second) { issue(p + ".id", "duplicate spring id " + std::to_string(e.id)); continue; }
      bool valid = true;
      if (!coords_.count(e.node1)) { issue(p + ".node1", "references unknown node " + std::to_string(e.node1)); valid = false; }
      if (!coords_.count(e.node2)) { issue(p + ".node2", "references unknown node " + std::to_string(e.node2)); valid = false; }
      if (valid && e.node1 == e.node2) { issue(p, "node1 and node2 must be different"); valid = false; }
      if (!finite(e.local_x)) { issue(p + ".local_x", "must be finite"); valid = false; }
      if (!finite(e.reference_y)) { issue(p + ".reference_y", "must be finite"); valid = false; }
      if (!springStiffnessValid(e.stiffness)) { issue(p + ".stiffness", "must be finite and non-negative"); valid = false; }
      if (!valid) continue;
      try { checkSpringNodesCoincident(coords_.at(e.node1), coords_.at(e.node2)); }
      catch (const std::exception& ex) { issue(p, ex.what()); }
      try { springAxes(e.local_x, e.reference_y); }
      catch (const std::exception& ex) { issue(p, std::string("invalid orientation: ") + ex.what()); }
    }
  }
  void restraints(const Model&, const Restraints& rs) {
    for (const auto& [node, flags] : rs) {
      (void)flags;
      if (!coords_.count(node)) issue("restraints[" + std::to_string(node) + "]", "references unknown node " + std::to_string(node));
    }
  }
  void loadCase(const Model&, const LoadCase& lc, const std::string& p) {
    if (lc.name.empty()) issue(p + ".name", "must not be empty");
    if (!std::isfinite(lc.self_weight)) issue(p + ".self_weight", "must be finite");
    for (std::size_t i = 0; i < lc.nodal.size(); ++i) {
      const auto& l = lc.nodal[i];
      const std::string lp = p + ".nodal[" + std::to_string(i) + "]";
      if (!coords_.count(l.node)) issue(lp + ".node", "references unknown node " + std::to_string(l.node));
      if (!l.values.allFinite()) issue(lp + ".values", "must be finite");
    }
    for (std::size_t i = 0; i < lc.member.size(); ++i) {
      const auto& l = lc.member[i];
      const std::string lp = p + ".member[" + std::to_string(i) + "]";
      if (!beamIds_.count(l.element)) issue(lp + ".element", "references unknown beam " + std::to_string(l.element));
      if (l.direction < 0 || l.direction > 2) issue(lp + ".direction", "must be 0, 1 or 2");
      if (!std::isfinite(l.w1) || !std::isfinite(l.w2)) issue(lp, "w1/w2 must be finite");
      if (!std::isfinite(l.start) || !std::isfinite(l.end) || !(l.start >= 0 && l.end <= 1 && l.start < l.end)) issue(lp, "loaded segment must satisfy 0 <= start < end <= 1");
      if (l.projected && l.axes != LoadAxes::Global) issue(lp + ".projected", "is only allowed with global axes");
      if (l.projected && l.kind != MemberLoadKind::Force) issue(lp + ".projected", "is only allowed for kind = Force");
    }
    for (std::size_t i = 0; i < lc.point.size(); ++i) {
      const auto& l = lc.point[i];
      const std::string lp = p + ".point[" + std::to_string(i) + "]";
      if (!beamIds_.count(l.element)) issue(lp + ".element", "references unknown beam " + std::to_string(l.element));
      if (l.direction < 0 || l.direction > 2) issue(lp + ".direction", "must be 0, 1 or 2");
      if (!std::isfinite(l.value)) issue(lp + ".value", "must be finite");
      if (!std::isfinite(l.position) || !(l.position >= 0 && l.position <= 1)) issue(lp + ".position", "must satisfy 0 <= position <= 1");
    }
  }
  void combinations(const std::vector<LoadCase>& cases, const std::vector<LoadCombination>& combos) {
    for (std::size_t i = 0; i < combos.size(); ++i) {
      const auto& cb = combos[i];
      const std::string p = "combinations[" + std::to_string(i) + "]";
      if (cb.name.empty()) issue(p + ".name", "must not be empty");
      if (cb.terms.empty()) issue(p + ".terms", "must not be empty");
      for (std::size_t k = 0; k < cb.terms.size(); ++k) {
        const auto& [idx, factor] = cb.terms[k];
        const std::string tp = p + ".terms[" + std::to_string(k) + "]";
        if (idx >= cases.size()) issue(tp + ".case", "references unknown load case index " + std::to_string(idx));
        if (!std::isfinite(factor)) issue(tp + ".factor", "must be finite");
      }
    }
  }
};

} // namespace detail

inline std::vector<ModelIssue> validateModel(const Model& m, const Restraints& rs = {}, const std::vector<LoadCase>& cases = {}, const std::vector<LoadCombination>& combos = {}) {
  detail::ModelValidator v;
  v.run(m, rs, cases, combos);
  return v.issues;
}

inline void validateModelOrThrow(const Model& m, const Restraints& rs = {}, const std::vector<LoadCase>& cases = {}, const std::vector<LoadCombination>& combos = {}) {
  auto issues = validateModel(m, rs, cases, combos);
  if (!issues.empty()) throw ModelValidationError(std::move(issues));
}

} // namespace kopruq
