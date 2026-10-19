#pragma once
// JSON model input/output, format "kopruq-model" version 1 (schemas/model.schema.json).
//
// The reader validates structure, types, references and value ranges and
// reports every problem with a JSON path ("$.beams[2].material"), instead of
// stopping at the first one. Unknown keys are rejected so that typos do not
// silently fall back to defaults. Units are the solver's canonical units
// (m, kN, rad); no unit conversion is performed.
#include "../solvers/LinearStaticSolver.hpp"
#include "../solvers/ResponseSpectrumSolver.hpp"
#include "../elements/Truss3D.hpp"
#include "../elements/SpringLink.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kopruq::io {

using json = nlohmann::json;

inline constexpr const char* model_schema_name = "kopruq-model";
inline constexpr int model_schema_version = 1;

// Keys accepted in each JSON object. io_tests checks these against schemas/model.schema.json.
struct KeySpec { std::vector<std::string> required, optional; };
namespace model_keys {
inline const KeySpec top{{"schema","schema_version","nodes","materials","sections","beams","supports"},{"units","trusses","springs","rigid_links","load_cases","combinations","response_spectra"}};
inline const KeySpec units{{},{"length","force","angle"}};
inline const KeySpec node{{"id","x","y","z"},{}};
inline const KeySpec material{{"id","elastic_modulus","poisson_ratio"},{"unit_weight","thermal_expansion"}};
inline const KeySpec section{{"id","area","Iy","Iz","J"},{"Asy","Asz"}};
inline const KeySpec beam{{"id","nodes","material","section"},{"reference_y","releases","offset1","offset2"}};
// A truss is read as a Beam3DConnection with every rotational DOF released (kopruq/elements/Truss3D.hpp);
// reference_y/releases are not accepted here since the truss's orientation and release pattern are implicit.
inline const KeySpec truss{{"id","nodes","material","section"},{}};
inline const KeySpec spring{{"id","nodes","stiffness"},{"local_x","reference_y"}};
inline const KeySpec rigid_link{{"id","master","slave"},{}};
inline const KeySpec releases{{},{"start","end"}};
inline const KeySpec support{{"node","restraints"},{}};
inline const KeySpec load_case{{"name"},{"self_weight","nodal","member","point","thermal"}};
inline const KeySpec nodal_load{{"node","values"},{}};
inline const KeySpec member_load{{"element","axes","direction","w1"},{"w2","start","end","projected","kind"}};
inline const KeySpec point_load{{"element","axes","direction","value","position"},{"kind"}};
inline const KeySpec member_thermal_load{{"element"},{"uniform_delta_t","gradient_y_delta_t","gradient_y_depth","gradient_z_delta_t","gradient_z_depth"}};
inline const KeySpec combination{{"name","terms"},{}};
inline const KeySpec combination_term{{"case","factor"},{}};
inline const KeySpec response_spectrum_case{{"name","direction","spectrum"},{"num_modes"}};
inline const KeySpec spectrum_point{{"period","sa"},{}};
}

struct InputIssue { std::string path, message; };

class ModelInputError : public std::runtime_error {
 public:
  std::vector<InputIssue> issues;
  explicit ModelInputError(std::vector<InputIssue> list) : std::runtime_error(format(list)), issues(std::move(list)) {}
  static std::string format(const std::vector<InputIssue>& list) {
    std::string s = "invalid model (" + std::to_string(list.size()) + " issue" + (list.size()==1 ? "" : "s") + ")";
    for (const auto& i : list) s += "\n  " + i.path + ": " + i.message;
    return s;
  }
};

struct ResponseSpectrumCase { std::string name; ResponseSpectrumInput input; };

struct ModelInput {
  Model model;
  LinearStaticSolver::Restraints restraints;
  std::vector<LoadCase> load_cases;
  std::vector<LoadCombination> combinations;
  std::vector<ResponseSpectrumCase> response_spectra;
};

namespace detail {

inline const char* axisName(int d) { return d==0 ? "x" : d==1 ? "y" : "z"; }
inline const char* kindName(MemberLoadKind k) { return k==MemberLoadKind::Force ? "force" : "moment"; }

class ModelReader {
 public:
  std::vector<InputIssue> issues;

  ModelInput read(const json& root) {
    ModelInput in;
    const std::string R = "$";
    if (!object(root, R, model_keys::top)) return in;
    readHeader(root, R);
    readNodes(root, R, in);
    readMaterials(root, R, in);
    readSections(root, R, in);
    readBeams(root, R, in);
    readTrusses(root, R, in);
    readSprings(root, R, in);
    readRigidLinks(root, R, in);
    readSupports(root, R, in);
    checkRigidLinkRestraints(in);
    readLoadCases(root, R, in);
    readCombinations(root, R, in);
    readResponseSpectra(root, R, in);
    return in;
  }

 private:
  std::set<Id> nodeIds_, materialIds_, sectionIds_, beamIds_, springIds_;
  std::map<Id, Id> rigidSlaveToMaster_;
  std::unordered_map<Id, Eigen::Vector3d> coords_;
  std::unordered_map<Id, Material> materials_;
  std::unordered_map<Id, Section> sections_;
  std::unordered_map<std::string, std::size_t> caseIndex_;

  void issue(const std::string& p, std::string m) { issues.push_back({p, std::move(m)}); }
  static std::string at(const std::string& p, std::size_t i) { return p + "[" + std::to_string(i) + "]"; }
  static std::string key(const std::string& p, const char* k) { return p + "." + k; }
  static const json* field(const json& j, const char* k) { auto it = j.find(k); return it == j.end() ? nullptr : &*it; }

  // Type, required keys and unknown keys of one object; false if it is not an object.
  bool object(const json& j, const std::string& p, const KeySpec& spec) {
    if (!j.is_object()) { issue(p, "must be an object"); return false; }
    for (const auto& r : spec.required) if (!j.contains(r)) issue(p, "missing required key '" + r + "'");
    for (auto it = j.begin(); it != j.end(); ++it) {
      const auto& k = it.key();
      bool known = std::find(spec.required.begin(), spec.required.end(), k) != spec.required.end()
                || std::find(spec.optional.begin(), spec.optional.end(), k) != spec.optional.end();
      if (!known) issue(p + "." + k, "unknown key");
    }
    return true;
  }
  // Optional array: absent -> nullptr without an issue.
  const json* array(const json& j, const std::string& p, const char* k) {
    const json* v = field(j, k);
    if (v && !v->is_array()) { issue(key(p, k), "must be an array"); return nullptr; }
    return v;
  }
  bool number(const json& j, const std::string& p, const char* k, Real& out) {
    const json* v = field(j, k);
    if (!v) return false;
    if (!v->is_number()) { issue(key(p, k), "must be a number"); return false; }
    Real x = v->get<Real>();
    if (!std::isfinite(x)) { issue(key(p, k), "must be finite"); return false; }
    out = x; return true;
  }
  // Positive integer value; nlohmann stores text-parsed integers as unsigned but C++-built ones as signed.
  static bool positiveInteger(const json& v, std::uint64_t& out) {
    if (v.is_number_unsigned()) { out = v.get<std::uint64_t>(); return out > 0; }
    if (v.is_number_integer()) { std::int64_t s = v.get<std::int64_t>(); out = static_cast<std::uint64_t>(s); return s > 0; }
    return false;
  }
  bool idValue(const json& v, const std::string& p, Id& out) {
    std::uint64_t x{};
    if (!positiveInteger(v, x)) { issue(p, "must be a positive integer"); return false; }
    out = static_cast<Id>(x); return true;
  }
  bool id(const json& j, const std::string& p, const char* k, Id& out) {
    const json* v = field(j, k);
    return v && idValue(*v, key(p, k), out);
  }
  bool boolean(const json& j, const std::string& p, const char* k, bool& out) {
    const json* v = field(j, k);
    if (!v) return false;
    if (!v->is_boolean()) { issue(key(p, k), "must be true or false"); return false; }
    out = v->get<bool>(); return true;
  }
  bool string(const json& j, const std::string& p, const char* k, std::string& out) {
    const json* v = field(j, k);
    if (!v) return false;
    if (!v->is_string()) { issue(key(p, k), "must be a string"); return false; }
    out = v->get<std::string>(); return true;
  }
  bool numbers(const json& j, const std::string& p, const char* k, std::size_t n, std::vector<Real>& out) {
    const json* v = field(j, k);
    if (!v) return false;
    if (!v->is_array() || v->size() != n) { issue(key(p, k), "must be an array of " + std::to_string(n) + " numbers"); return false; }
    out.clear();
    for (std::size_t i = 0; i < n; ++i) {
      const auto& e = (*v)[i];
      if (!e.is_number() || !std::isfinite(e.get<Real>())) { issue(at(key(p, k), i), "must be a finite number"); return false; }
      out.push_back(e.get<Real>());
    }
    return true;
  }
  bool positive(const std::string& p, const char* k, Real x) {
    if (x > 0) return true;
    issue(key(p, k), "must be greater than 0"); return false;
  }
  bool nonNegative(const std::string& p, const char* k, Real x) {
    if (x >= 0) return true;
    issue(key(p, k), "must be non-negative"); return false;
  }

  void readHeader(const json& root, const std::string& R) {
    std::string schema;
    if (string(root, R, "schema", schema) && schema != model_schema_name)
      issue(key(R, "schema"), std::string("must be \"") + model_schema_name + "\"");
    std::uint64_t version{};
    if (const json* v = field(root, "schema_version"))
      if (!positiveInteger(*v, version) || version != static_cast<std::uint64_t>(model_schema_version))
        issue(key(R, "schema_version"), "unsupported version; this solver reads version " + std::to_string(model_schema_version));
    if (const json* u = field(root, "units")) {
      const std::string P = key(R, "units");
      if (object(*u, P, model_keys::units)) {
        const std::pair<const char*, const char*> canonical[] = {{"length","m"},{"force","kN"},{"angle","rad"}};
        for (const auto& [k, value] : canonical) {
          std::string s;
          if (string(*u, P, k, s) && s != value)
            issue(key(P, k), std::string("must be \"") + value + "\" (no unit conversion is performed)");
        }
      }
    }
  }

  void readNodes(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "nodes");
    if (!a) return;
    if (a->empty()) issue(key(R, "nodes"), "must contain at least one node");
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "nodes"), i);
      const json& n = (*a)[i];
      if (!object(n, p, model_keys::node)) continue;
      Id nid{}; Real x{}, y{}, z{};
      bool ok = id(n, p, "id", nid);
      ok = number(n, p, "x", x) && ok;
      ok = number(n, p, "y", y) && ok;
      ok = number(n, p, "z", z) && ok;
      if (!ok) continue;
      if (!nodeIds_.insert(nid).second) { issue(key(p, "id"), "duplicate node id " + std::to_string(nid)); continue; }
      in.model.nodes.emplace_back(nid, x, y, z);
      coords_[nid] = Eigen::Vector3d(x, y, z);
    }
  }

  void readMaterials(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "materials");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "materials"), i);
      const json& m = (*a)[i];
      if (!object(m, p, model_keys::material)) continue;
      Material mat;
      bool ok = id(m, p, "id", mat.id);
      ok = number(m, p, "elastic_modulus", mat.elastic_modulus) && ok;
      ok = number(m, p, "poisson_ratio", mat.poisson_ratio) && ok;
      if (field(m, "unit_weight")) ok = number(m, p, "unit_weight", mat.unit_weight) && ok;
      if (field(m, "thermal_expansion")) ok = number(m, p, "thermal_expansion", mat.thermal_expansion) && ok;
      if (!ok) continue;
      bool valid = positive(p, "elastic_modulus", mat.elastic_modulus);
      if (!(mat.poisson_ratio > -1 && mat.poisson_ratio < 0.5)) { issue(key(p, "poisson_ratio"), "must satisfy -1 < poisson_ratio < 0.5"); valid = false; }
      if (mat.unit_weight < 0) { issue(key(p, "unit_weight"), "must be >= 0"); valid = false; }
      if (!valid) continue;
      if (!materialIds_.insert(mat.id).second) { issue(key(p, "id"), "duplicate material id " + std::to_string(mat.id)); continue; }
      in.model.materials.push_back(mat);
      materials_[mat.id] = mat;
    }
  }

  void readSections(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "sections");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "sections"), i);
      const json& s = (*a)[i];
      if (!object(s, p, model_keys::section)) continue;
      Section sec;
      bool ok = id(s, p, "id", sec.id);
      ok = number(s, p, "area", sec.area) && ok;
      ok = number(s, p, "Iy", sec.Iy) && ok;
      ok = number(s, p, "Iz", sec.Iz) && ok;
      ok = number(s, p, "J", sec.J) && ok;
      if (field(s, "Asy")) ok = number(s, p, "Asy", sec.Asy) && ok;
      if (field(s, "Asz")) ok = number(s, p, "Asz", sec.Asz) && ok;
      if (!ok) continue;
      bool valid = positive(p, "area", sec.area);
      valid = positive(p, "Iy", sec.Iy) && valid;
      valid = positive(p, "Iz", sec.Iz) && valid;
      valid = positive(p, "J", sec.J) && valid;
      valid = nonNegative(p, "Asy", sec.Asy) && valid;
      valid = nonNegative(p, "Asz", sec.Asz) && valid;
      if (!valid) continue;
      if (!sectionIds_.insert(sec.id).second) { issue(key(p, "id"), "duplicate section id " + std::to_string(sec.id)); continue; }
      in.model.sections.push_back(sec);
      sections_[sec.id] = sec;
    }
  }

  void readBeams(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "beams");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "beams"), i);
      const json& b = (*a)[i];
      if (!object(b, p, model_keys::beam)) continue;
      Beam3DConnection e;
      bool ok = id(b, p, "id", e.id);
      if (const json* ns = field(b, "nodes")) {
        if (!ns->is_array() || ns->size() != 2) { issue(key(p, "nodes"), "must be an array of 2 node ids"); ok = false; }
        else {
          ok = idValue((*ns)[0], at(key(p, "nodes"), 0), e.node1) && ok;
          ok = idValue((*ns)[1], at(key(p, "nodes"), 1), e.node2) && ok;
        }
      }
      ok = id(b, p, "material", e.material) && ok;
      ok = id(b, p, "section", e.section) && ok;
      if (field(b, "reference_y")) {
        std::vector<Real> r;
        if (numbers(b, p, "reference_y", 3, r)) e.reference_y = Eigen::Vector3d(r[0], r[1], r[2]); else ok = false;
      }
      if (const json* rl = field(b, "releases")) ok = readReleases(*rl, key(p, "releases"), e.releases) && ok;
      if (field(b, "offset1")) {
        std::vector<Real> r;
        if (numbers(b, p, "offset1", 3, r)) e.offset1 = Eigen::Vector3d(r[0], r[1], r[2]); else ok = false;
      }
      if (field(b, "offset2")) {
        std::vector<Real> r;
        if (numbers(b, p, "offset2", 3, r)) e.offset2 = Eigen::Vector3d(r[0], r[1], r[2]); else ok = false;
      }
      if (!ok) continue;
      bool valid = true;
      for (int k = 0; k < 2; ++k) {
        Id n = k == 0 ? e.node1 : e.node2;
        if (!nodeIds_.count(n)) { issue(at(key(p, "nodes"), k), "references unknown node " + std::to_string(n)); valid = false; }
      }
      if (valid && e.node1 == e.node2) { issue(key(p, "nodes"), "must reference two different nodes"); valid = false; }
      if (!materialIds_.count(e.material)) { issue(key(p, "material"), "references unknown material " + std::to_string(e.material)); valid = false; }
      if (!sectionIds_.count(e.section)) { issue(key(p, "section"), "references unknown section " + std::to_string(e.section)); valid = false; }
      if (valid) {
        try { Beam3D::axes(Beam3DGeometry{coords_[e.node1] + e.offset1, coords_[e.node2] + e.offset2, e.reference_y}); }
        catch (const std::exception& ex) { issue(p, std::string("invalid geometry: ") + ex.what()); valid = false; }
      }
      if (valid && anyReleased(e.releases)) {
        const Material& mat = materials_.at(e.material); const Section& sec = sections_.at(e.section);
        Beam3DProperties bp{mat.elastic_modulus, mat.elastic_modulus / (2 * (1 + mat.poisson_ratio)), sec.area, sec.Iy, sec.Iz, sec.J, sec.Asy, sec.Asz};
        try { condensedStiffness(Beam3D::localStiffness(((coords_[e.node2] + e.offset2) - (coords_[e.node1] + e.offset1)).norm(), bp), e.releases); }
        catch (const std::exception& ex) { issue(key(p, "releases"), ex.what()); valid = false; }
      }
      if (!valid) continue;
      if (!beamIds_.insert(e.id).second) { issue(key(p, "id"), "duplicate beam id " + std::to_string(e.id)); continue; }
      in.model.beams.push_back(e);
    }
  }

  void readTrusses(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "trusses");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "trusses"), i);
      const json& b = (*a)[i];
      if (!object(b, p, model_keys::truss)) continue;
      Id eid{}, n1{}, n2{}, mat{}, sec{};
      bool ok = id(b, p, "id", eid);
      if (const json* ns = field(b, "nodes")) {
        if (!ns->is_array() || ns->size() != 2) { issue(key(p, "nodes"), "must be an array of 2 node ids"); ok = false; }
        else {
          ok = idValue((*ns)[0], at(key(p, "nodes"), 0), n1) && ok;
          ok = idValue((*ns)[1], at(key(p, "nodes"), 1), n2) && ok;
        }
      }
      ok = id(b, p, "material", mat) && ok;
      ok = id(b, p, "section", sec) && ok;
      if (!ok) continue;
      bool valid = true;
      for (int k = 0; k < 2; ++k) {
        Id n = k == 0 ? n1 : n2;
        if (!nodeIds_.count(n)) { issue(at(key(p, "nodes"), k), "references unknown node " + std::to_string(n)); valid = false; }
      }
      if (valid && n1 == n2) { issue(key(p, "nodes"), "must reference two different nodes"); valid = false; }
      if (!materialIds_.count(mat)) { issue(key(p, "material"), "references unknown material " + std::to_string(mat)); valid = false; }
      if (!sectionIds_.count(sec)) { issue(key(p, "section"), "references unknown section " + std::to_string(sec)); valid = false; }
      if (!valid) continue;
      if (!beamIds_.insert(eid).second) { issue(key(p, "id"), "duplicate beam/truss id " + std::to_string(eid)); continue; }
      in.model.beams.push_back(makeTrussConnection(eid, n1, n2, mat, sec, coords_[n1], coords_[n2]));
    }
  }

  void readSprings(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "springs");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "springs"), i);
      const json& b = (*a)[i];
      if (!object(b, p, model_keys::spring)) continue;
      SpringConnection e;
      bool ok = id(b, p, "id", e.id);
      if (const json* ns = field(b, "nodes")) {
        if (!ns->is_array() || ns->size() != 2) { issue(key(p, "nodes"), "must be an array of 2 node ids"); ok = false; }
        else {
          ok = idValue((*ns)[0], at(key(p, "nodes"), 0), e.node1) && ok;
          ok = idValue((*ns)[1], at(key(p, "nodes"), 1), e.node2) && ok;
        }
      }
      if (field(b, "local_x")) {
        std::vector<Real> r;
        if (numbers(b, p, "local_x", 3, r)) e.local_x = Eigen::Vector3d(r[0], r[1], r[2]); else ok = false;
      }
      if (field(b, "reference_y")) {
        std::vector<Real> r;
        if (numbers(b, p, "reference_y", 3, r)) e.reference_y = Eigen::Vector3d(r[0], r[1], r[2]); else ok = false;
      }
      if (const json* st = field(b, "stiffness")) {
        if (!st->is_array() || st->size() != 6) { issue(key(p, "stiffness"), "must be an array of 6 numbers (kx,ky,kz,krx,kry,krz)"); ok = false; }
        else for (std::size_t k = 0; k < 6; ++k) {
          if (!(*st)[k].is_number()) { issue(at(key(p, "stiffness"), k), "must be a number"); ok = false; }
          else e.stiffness[k] = (*st)[k].get<Real>();
        }
      } else ok = false; // missing required key already reported by object()
      if (!ok) continue;
      bool valid = true;
      for (int k = 0; k < 2; ++k) {
        Id n = k == 0 ? e.node1 : e.node2;
        if (!nodeIds_.count(n)) { issue(at(key(p, "nodes"), k), "references unknown node " + std::to_string(n)); valid = false; }
      }
      if (valid && e.node1 == e.node2) { issue(key(p, "nodes"), "must reference two different nodes"); valid = false; }
      if (!springStiffnessValid(e.stiffness)) { issue(key(p, "stiffness"), "must be finite and non-negative"); valid = false; }
      if (valid) {
        try { checkSpringNodesCoincident(coords_[e.node1], coords_[e.node2]); }
        catch (const std::exception& ex) { issue(p, ex.what()); valid = false; }
      }
      if (valid) {
        try { springAxes(e.local_x, e.reference_y); }
        catch (const std::exception& ex) { issue(p, std::string("invalid orientation: ") + ex.what()); valid = false; }
      }
      if (!valid) continue;
      if (!springIds_.insert(e.id).second) { issue(key(p, "id"), "duplicate spring id " + std::to_string(e.id)); continue; }
      in.model.springs.push_back(e);
    }
  }

  void readRigidLinks(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "rigid_links");
    if (!a) return;
    std::set<Id> linkIds;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "rigid_links"), i);
      const json& b = (*a)[i];
      if (!object(b, p, model_keys::rigid_link)) continue;
      RigidLink l;
      bool ok = id(b, p, "id", l.id);
      ok = id(b, p, "master", l.master) && ok;
      ok = id(b, p, "slave", l.slave) && ok;
      if (!ok) continue;
      bool valid = true;
      if (!nodeIds_.count(l.master)) { issue(key(p, "master"), "references unknown node " + std::to_string(l.master)); valid = false; }
      if (!nodeIds_.count(l.slave)) { issue(key(p, "slave"), "references unknown node " + std::to_string(l.slave)); valid = false; }
      if (valid && l.master == l.slave) { issue(p, "master and slave must be different nodes"); valid = false; }
      if (!valid) continue;
      if (!linkIds.insert(l.id).second) { issue(key(p, "id"), "duplicate rigid link id " + std::to_string(l.id)); continue; }
      if (rigidSlaveToMaster_.count(l.slave)) { issue(key(p, "slave"), "node " + std::to_string(l.slave) + " is a slave of more than one rigid link"); continue; }
      rigidSlaveToMaster_[l.slave] = l.master;
      in.model.rigid_links.push_back(l);
    }
    for (const auto& [slave, master] : rigidSlaveToMaster_)
      if (rigidSlaveToMaster_.count(master)) issue("rigid_links", "chaining: node " + std::to_string(master) + " is both a master and a slave");
  }

  void checkRigidLinkRestraints(const ModelInput& in) {
    for (const auto& [slave, master] : rigidSlaveToMaster_) {
      (void)master;
      auto it = in.restraints.find(slave);
      if (it == in.restraints.end()) continue;
      for (bool f : it->second) if (f) { issue("supports", "node " + std::to_string(slave) + " is a rigid-link slave and cannot carry its own restraints"); break; }
    }
  }

  bool readReleases(const json& j, const std::string& p, Releases& out) {
    if (!object(j, p, model_keys::releases)) return false;
    bool ok = true;
    const char* ends[2] = {"start", "end"};
    for (int e = 0; e < 2; ++e) {
      const json* v = field(j, ends[e]);
      if (!v) continue;
      if (!v->is_array() || v->size() != 6) { issue(key(p, ends[e]), "must be an array of 6 booleans (UX, UY, UZ, RX, RY, RZ, member-local)"); ok = false; continue; }
      for (std::size_t k = 0; k < 6; ++k) {
        if (!(*v)[k].is_boolean()) { issue(at(key(p, ends[e]), k), "must be true or false"); ok = false; }
        else out[6 * e + k] = (*v)[k].get<bool>();
      }
    }
    return ok;
  }

  void readSupports(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "supports");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "supports"), i);
      const json& s = (*a)[i];
      if (!object(s, p, model_keys::support)) continue;
      Id node{};
      bool ok = id(s, p, "node", node);
      std::array<bool, 6> r{};
      if (const json* v = field(s, "restraints")) {
        if (!v->is_array() || v->size() != 6) { issue(key(p, "restraints"), "must be an array of 6 booleans (UX, UY, UZ, RX, RY, RZ)"); ok = false; }
        else for (std::size_t k = 0; k < 6; ++k) {
          if (!(*v)[k].is_boolean()) { issue(at(key(p, "restraints"), k), "must be true or false"); ok = false; }
          else r[k] = (*v)[k].get<bool>();
        }
      }
      if (!ok) continue;
      if (!nodeIds_.count(node)) { issue(key(p, "node"), "references unknown node " + std::to_string(node)); continue; }
      if (!in.restraints.emplace(node, r).second) issue(key(p, "node"), "duplicate support for node " + std::to_string(node));
    }
  }

  void readLoadCases(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "load_cases");
    if (!a) return;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "load_cases"), i);
      const json& c = (*a)[i];
      if (!object(c, p, model_keys::load_case)) continue;
      LoadCase lc;
      bool ok = true;
      if (string(c, p, "name", lc.name)) {
        if (lc.name.empty()) { issue(key(p, "name"), "must not be empty"); ok = false; }
        else if (caseIndex_.count(lc.name)) { issue(key(p, "name"), "duplicate load case name \"" + lc.name + "\""); ok = false; }
      } else ok = false;
      if (field(c, "self_weight")) ok = number(c, p, "self_weight", lc.self_weight) && ok;
      if (const json* nl = array(c, p, "nodal"))
        for (std::size_t k = 0; k < nl->size(); ++k) ok = readNodalLoad((*nl)[k], at(key(p, "nodal"), k), lc) && ok;
      if (const json* ml = array(c, p, "member"))
        for (std::size_t k = 0; k < ml->size(); ++k) ok = readMemberLoad((*ml)[k], at(key(p, "member"), k), lc) && ok;
      if (const json* pl = array(c, p, "point"))
        for (std::size_t k = 0; k < pl->size(); ++k) ok = readPointLoad((*pl)[k], at(key(p, "point"), k), lc) && ok;
      if (const json* tl = array(c, p, "thermal"))
        for (std::size_t k = 0; k < tl->size(); ++k) ok = readThermalLoad((*tl)[k], at(key(p, "thermal"), k), lc) && ok;
      if (!ok) continue;
      caseIndex_[lc.name] = in.load_cases.size();
      in.load_cases.push_back(std::move(lc));
    }
  }

  bool readNodalLoad(const json& j, const std::string& p, LoadCase& lc) {
    if (!object(j, p, model_keys::nodal_load)) return false;
    NodalLoad l;
    bool ok = id(j, p, "node", l.node);
    std::vector<Real> v;
    if (numbers(j, p, "values", 6, v)) for (int i = 0; i < 6; ++i) l.values(i) = v[i]; else ok = false;
    if (ok && !nodeIds_.count(l.node)) { issue(key(p, "node"), "references unknown node " + std::to_string(l.node)); ok = false; }
    if (ok) lc.nodal.push_back(l);
    return ok;
  }

  // "axes", "direction" and the optional "kind" shared by member and point loads.
  bool readAction(const json& j, const std::string& p, LoadAxes& axesOut, int& dirOut, MemberLoadKind& kindOut) {
    bool ok = true;
    std::string axes, dir, kind;
    if (string(j, p, "axes", axes)) {
      if (axes == "local") axesOut = LoadAxes::Local;
      else if (axes == "global") axesOut = LoadAxes::Global;
      else { issue(key(p, "axes"), "must be \"local\" or \"global\""); ok = false; }
    } else ok = false;
    if (string(j, p, "direction", dir)) {
      if (dir == "x") dirOut = 0; else if (dir == "y") dirOut = 1; else if (dir == "z") dirOut = 2;
      else { issue(key(p, "direction"), "must be \"x\", \"y\" or \"z\""); ok = false; }
    } else ok = false;
    if (field(j, "kind")) {
      if (string(j, p, "kind", kind)) {
        if (kind == "force") kindOut = MemberLoadKind::Force;
        else if (kind == "moment") kindOut = MemberLoadKind::Moment;
        else { issue(key(p, "kind"), "must be \"force\" or \"moment\""); ok = false; }
      } else ok = false;
    }
    return ok;
  }

  bool readMemberLoad(const json& j, const std::string& p, LoadCase& lc) {
    if (!object(j, p, model_keys::member_load)) return false;
    MemberLoad l;
    bool ok = id(j, p, "element", l.element);
    ok = readAction(j, p, l.axes, l.direction, l.kind) && ok;
    ok = number(j, p, "w1", l.w1) && ok;
    l.w2 = l.w1;
    if (field(j, "w2")) ok = number(j, p, "w2", l.w2) && ok;
    if (field(j, "start")) ok = number(j, p, "start", l.start) && ok;
    if (field(j, "end")) ok = number(j, p, "end", l.end) && ok;
    if (field(j, "projected")) ok = boolean(j, p, "projected", l.projected) && ok;
    if (!ok) return false;
    if (!beamIds_.count(l.element)) { issue(key(p, "element"), "references unknown beam " + std::to_string(l.element)); ok = false; }
    if (!(l.start >= 0 && l.end <= 1 && l.start < l.end)) { issue(p, "loaded segment must satisfy 0 <= start < end <= 1"); ok = false; }
    if (l.projected && l.axes != LoadAxes::Global) { issue(key(p, "projected"), "is only allowed with \"axes\": \"global\""); ok = false; }
    if (l.projected && l.kind != MemberLoadKind::Force) { issue(key(p, "projected"), "is only allowed for \"kind\": \"force\""); ok = false; }
    if (ok) lc.member.push_back(l);
    return ok;
  }

  bool readPointLoad(const json& j, const std::string& p, LoadCase& lc) {
    if (!object(j, p, model_keys::point_load)) return false;
    MemberPointLoad l;
    bool ok = id(j, p, "element", l.element);
    ok = readAction(j, p, l.axes, l.direction, l.kind) && ok;
    ok = number(j, p, "value", l.value) && ok;
    ok = number(j, p, "position", l.position) && ok;
    if (!ok) return false;
    if (!beamIds_.count(l.element)) { issue(key(p, "element"), "references unknown beam " + std::to_string(l.element)); ok = false; }
    if (!(l.position >= 0 && l.position <= 1)) { issue(key(p, "position"), "must satisfy 0 <= position <= 1"); ok = false; }
    if (ok) lc.point.push_back(l);
    return ok;
  }

  bool readThermalLoad(const json& j, const std::string& p, LoadCase& lc) {
    if (!object(j, p, model_keys::member_thermal_load)) return false;
    MemberThermalLoad l;
    bool ok = id(j, p, "element", l.element);
    if (field(j, "uniform_delta_t")) ok = number(j, p, "uniform_delta_t", l.uniform_delta_t) && ok;
    if (field(j, "gradient_y_delta_t")) ok = number(j, p, "gradient_y_delta_t", l.gradient_y_delta_t) && ok;
    if (field(j, "gradient_y_depth")) ok = number(j, p, "gradient_y_depth", l.gradient_y_depth) && ok;
    if (field(j, "gradient_z_delta_t")) ok = number(j, p, "gradient_z_delta_t", l.gradient_z_delta_t) && ok;
    if (field(j, "gradient_z_depth")) ok = number(j, p, "gradient_z_depth", l.gradient_z_depth) && ok;
    if (!ok) return false;
    if (!beamIds_.count(l.element)) { issue(key(p, "element"), "references unknown beam " + std::to_string(l.element)); ok = false; }
    if (l.gradient_y_delta_t != 0 && !(l.gradient_y_depth > 0)) { issue(key(p, "gradient_y_depth"), "must be positive when gradient_y_delta_t is nonzero"); ok = false; }
    if (l.gradient_z_delta_t != 0 && !(l.gradient_z_depth > 0)) { issue(key(p, "gradient_z_depth"), "must be positive when gradient_z_delta_t is nonzero"); ok = false; }
    if (ok) lc.thermal.push_back(l);
    return ok;
  }

  void readCombinations(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "combinations");
    if (!a) return;
    std::set<std::string> names;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "combinations"), i);
      const json& c = (*a)[i];
      if (!object(c, p, model_keys::combination)) continue;
      LoadCombination cb;
      bool ok = true;
      if (string(c, p, "name", cb.name)) {
        if (cb.name.empty()) { issue(key(p, "name"), "must not be empty"); ok = false; }
        else if (!names.insert(cb.name).second) { issue(key(p, "name"), "duplicate combination name \"" + cb.name + "\""); ok = false; }
      } else ok = false;
      if (const json* t = field(c, "terms")) {
        if (!t->is_array() || t->empty()) { issue(key(p, "terms"), "must be a non-empty array"); ok = false; }
        else for (std::size_t k = 0; k < t->size(); ++k) {
          const std::string tp = at(key(p, "terms"), k);
          const json& term = (*t)[k];
          if (!object(term, tp, model_keys::combination_term)) { ok = false; continue; }
          std::string cname; Real factor{};
          bool tok = string(term, tp, "case", cname);
          tok = number(term, tp, "factor", factor) && tok;
          if (!tok) { ok = false; continue; }
          auto it = caseIndex_.find(cname);
          if (it == caseIndex_.end()) { issue(key(tp, "case"), "references unknown load case \"" + cname + "\""); ok = false; continue; }
          cb.terms.emplace_back(it->second, factor);
        }
      } else ok = false;
      if (ok) in.combinations.push_back(std::move(cb));
    }
  }

  void readResponseSpectra(const json& root, const std::string& R, ModelInput& in) {
    const json* a = array(root, R, "response_spectra");
    if (!a) return;
    std::set<std::string> names;
    for (std::size_t i = 0; i < a->size(); ++i) {
      const std::string p = at(key(R, "response_spectra"), i);
      const json& c = (*a)[i];
      if (!object(c, p, model_keys::response_spectrum_case)) continue;
      ResponseSpectrumCase rc;
      bool ok = true;
      if (string(c, p, "name", rc.name)) {
        if (rc.name.empty()) { issue(key(p, "name"), "must not be empty"); ok = false; }
        else if (!names.insert(rc.name).second) { issue(key(p, "name"), "duplicate response spectrum name \"" + rc.name + "\""); ok = false; }
      } else ok = false;
      std::string dir;
      if (string(c, p, "direction", dir)) {
        if (dir == "x") rc.input.direction = 0; else if (dir == "y") rc.input.direction = 1; else if (dir == "z") rc.input.direction = 2;
        else { issue(key(p, "direction"), "must be \"x\", \"y\" or \"z\""); ok = false; }
      } else ok = false;
      rc.input.num_modes = 6;
      if (field(c, "num_modes")) {
        Real nm{};
        if (number(c, p, "num_modes", nm)) {
          if (!(nm >= 1) || nm != static_cast<int>(nm)) { issue(key(p, "num_modes"), "must be a positive integer"); ok = false; }
          else rc.input.num_modes = static_cast<int>(nm);
        } else ok = false;
      }
      if (const json* sp = field(c, "spectrum")) {
        if (!sp->is_array() || sp->size() < 2) { issue(key(p, "spectrum"), "must be an array of at least 2 points"); ok = false; }
        else {
          for (std::size_t k = 0; k < sp->size(); ++k) ok = readSpectrumPoint((*sp)[k], at(key(p, "spectrum"), k), rc.input) && ok;
          for (std::size_t k = 1; ok && k < rc.input.spectrum.size(); ++k)
            if (!(rc.input.spectrum[k].period_s > rc.input.spectrum[k - 1].period_s)) { issue(key(p, "spectrum"), "periods must be strictly increasing"); ok = false; }
        }
      } else ok = false;
      if (ok) in.response_spectra.push_back(std::move(rc));
    }
  }

  bool readSpectrumPoint(const json& j, const std::string& p, ResponseSpectrumInput& input) {
    if (!object(j, p, model_keys::spectrum_point)) return false;
    Real period{}, sa{};
    bool ok = number(j, p, "period", period);
    ok = number(j, p, "sa", sa) && ok;
    if (!ok) return false;
    if (!(period > 0)) { issue(key(p, "period"), "must be greater than 0"); ok = false; }
    if (!(sa >= 0)) { issue(key(p, "sa"), "must be >= 0"); ok = false; }
    if (ok) input.spectrum.push_back({period, sa});
    return ok;
  }
};

} // namespace detail

inline ModelInput readModel(const json& root) {
  detail::ModelReader r;
  ModelInput in = r.read(root);
  if (!r.issues.empty()) throw ModelInputError(std::move(r.issues));
  return in;
}

inline ModelInput readModelText(std::string_view text) {
  json root;
  try { root = json::parse(text.begin(), text.end()); }
  catch (const json::parse_error& e) { throw ModelInputError({{"$", std::string("invalid JSON: ") + e.what()}}); }
  return readModel(root);
}

inline json writeModel(const ModelInput& in) {
  json j;
  j["schema"] = model_schema_name;
  j["schema_version"] = model_schema_version;
  j["nodes"] = json::array();
  for (const auto& n : in.model.nodes) j["nodes"].push_back({{"id", n.id}, {"x", n.x}, {"y", n.y}, {"z", n.z}});
  j["materials"] = json::array();
  for (const auto& m : in.model.materials)
    j["materials"].push_back({{"id", m.id}, {"elastic_modulus", m.elastic_modulus}, {"poisson_ratio", m.poisson_ratio}, {"unit_weight", m.unit_weight}, {"thermal_expansion", m.thermal_expansion}});
  j["sections"] = json::array();
  for (const auto& s : in.model.sections) j["sections"].push_back({{"id", s.id}, {"area", s.area}, {"Iy", s.Iy}, {"Iz", s.Iz}, {"J", s.J}, {"Asy", s.Asy}, {"Asz", s.Asz}});
  j["beams"] = json::array();
  for (const auto& e : in.model.beams)
    j["beams"].push_back({{"id", e.id}, {"nodes", {e.node1, e.node2}}, {"material", e.material}, {"section", e.section},
                          {"reference_y", {e.reference_y.x(), e.reference_y.y(), e.reference_y.z()}},
                          {"releases", {{"start", std::array<bool, 6>{e.releases[0], e.releases[1], e.releases[2], e.releases[3], e.releases[4], e.releases[5]}},
                                        {"end", std::array<bool, 6>{e.releases[6], e.releases[7], e.releases[8], e.releases[9], e.releases[10], e.releases[11]}}}},
                          {"offset1", {e.offset1.x(), e.offset1.y(), e.offset1.z()}},
                          {"offset2", {e.offset2.x(), e.offset2.y(), e.offset2.z()}}});
  j["springs"] = json::array();
  for (const auto& e : in.model.springs)
    j["springs"].push_back({{"id", e.id}, {"nodes", {e.node1, e.node2}},
                            {"local_x", {e.local_x.x(), e.local_x.y(), e.local_x.z()}},
                            {"reference_y", {e.reference_y.x(), e.reference_y.y(), e.reference_y.z()}},
                            {"stiffness", {e.stiffness[0], e.stiffness[1], e.stiffness[2], e.stiffness[3], e.stiffness[4], e.stiffness[5]}}});
  j["rigid_links"] = json::array();
  for (const auto& l : in.model.rigid_links) j["rigid_links"].push_back({{"id", l.id}, {"master", l.master}, {"slave", l.slave}});
  j["supports"] = json::array();
  for (const auto& [node, r] : in.restraints) j["supports"].push_back({{"node", node}, {"restraints", r}});
  j["load_cases"] = json::array();
  for (const auto& lc : in.load_cases) {
    json c = {{"name", lc.name}, {"self_weight", lc.self_weight}, {"nodal", json::array()}, {"member", json::array()}, {"point", json::array()}, {"thermal", json::array()}};
    for (const auto& l : lc.nodal)
      c["nodal"].push_back({{"node", l.node}, {"values", {l.values(0), l.values(1), l.values(2), l.values(3), l.values(4), l.values(5)}}});
    for (const auto& l : lc.member)
      c["member"].push_back({{"element", l.element}, {"axes", l.axes == LoadAxes::Local ? "local" : "global"},
                             {"direction", detail::axisName(l.direction)}, {"w1", l.w1}, {"w2", l.w2},
                             {"start", l.start}, {"end", l.end}, {"projected", l.projected}, {"kind", detail::kindName(l.kind)}});
    for (const auto& l : lc.point)
      c["point"].push_back({{"element", l.element}, {"axes", l.axes == LoadAxes::Local ? "local" : "global"},
                            {"direction", detail::axisName(l.direction)}, {"kind", detail::kindName(l.kind)},
                            {"value", l.value}, {"position", l.position}});
    for (const auto& l : lc.thermal)
      c["thermal"].push_back({{"element", l.element}, {"uniform_delta_t", l.uniform_delta_t},
                              {"gradient_y_delta_t", l.gradient_y_delta_t}, {"gradient_y_depth", l.gradient_y_depth},
                              {"gradient_z_delta_t", l.gradient_z_delta_t}, {"gradient_z_depth", l.gradient_z_depth}});
    j["load_cases"].push_back(c);
  }
  j["combinations"] = json::array();
  for (const auto& cb : in.combinations) {
    json c = {{"name", cb.name}, {"terms", json::array()}};
    for (const auto& [idx, f] : cb.terms) c["terms"].push_back({{"case", in.load_cases.at(idx).name}, {"factor", f}});
    j["combinations"].push_back(c);
  }
  j["response_spectra"] = json::array();
  for (const auto& rc : in.response_spectra) {
    json c = {{"name", rc.name}, {"direction", detail::axisName(rc.input.direction)}, {"num_modes", rc.input.num_modes}, {"spectrum", json::array()}};
    for (const auto& pt : rc.input.spectrum) c["spectrum"].push_back({{"period", pt.period_s}, {"sa", pt.spectral_acceleration}});
    j["response_spectra"].push_back(c);
  }
  return j;
}

} // namespace kopruq::io
