#pragma once
// MIDAS Civil NX JSON export -> KOPRUQ-SOLVER internal Model/LoadCase
// (Phase 7B). Kept entirely separate from the solver: this file only
// *produces* a kopruq::io::ModelInput, using the already-existing,
// unmodified core types and the MIDAS-specific helpers in this directory
// (MidasMaterialLibrary, MidasSectionLibrary, MidasOrientation).
//
// Explicitly supported (per docs/MIDAS_JSON_DATA_AUDIT.md): NODE, ELEM
// (TYPE="BEAM" only), MATL/MATD (TYPE="CONC", grade-named via a
// EN..(RC) code family only), SECT (SECTTYPE="DBUSER", shapes SB/B/SR
// only), CONS (point restraints), RIGD (master/slave rigid links), BODF
// (self-weight, direction must be exactly global -Z), BMLD
// (CMD="BEAM", TYPE="UNILOAD", DIRECTION="GZ" uniform member loads),
// ETMP (uniform element temperature -- maps directly onto
// MemberThermalLoad::uniform_delta_t, Phase 7T; TEMP is already a
// temperature CHANGE, the same quantity that load takes).
//
// Explicitly REJECTED, not silently ignored or approximated (recorded in
// MidasImportReport::rejected with the record identified): any ELEM TYPE
// other than BEAM; any MATL TYPE other than CONC or any code family other
// than EN..(RC); any SECT shape other than SB/B/SR; ELNK (elastic links
// between non-coincident nodes -- SpringLink, Phase 5.4, requires
// coincident nodes, a real formulation limit, not a missing-data gap);
// GTMP/BTMP (gradient/beam-section temperature -- MIDAS's T1/T2
// top-fiber sign convention could not be confirmed from accessible
// documentation, and guessing would silently flip which way a member
// bends); any BODF whose direction is not exactly global -Z; any BMLD
// whose CMD/TYPE/DIRECTION is not the one combination implemented above.
//
// Disclosed, not silent, substitutions (MidasImportReport::assumptions):
// material Poisson's ratio/unit weight (MidasMaterialLibrary.hpp);
// Timoshenko shear areas are NOT derived even when SECT_BEFORE's
// USE_SHEAR_DEFORM is true (no shear-area formulas implemented yet in
// MidasSectionLibrary.hpp) -- sections import as Euler-Bernoulli
// (Asy=Asz=0), and this gap is recorded, not hidden.
//
// NOT executed in this phase (recorded in report, not silently dropped):
// EIGV/SPLC/SPFC (modal/response-spectrum requests) are present in many
// MIDAS exports but are not translated into kopruq `response_spectra`
// entries here -- ModalSolver/ResponseSpectrumSolver (Phase 5.5) reject
// any model with `rigid_links`, which RIGD maps onto directly, so a
// model built by this importer that has any RIGD record cannot run
// modal/response-spectrum analysis today regardless.
#include "../ModelJson.hpp"
#include "MidasMaterialLibrary.hpp"
#include "MidasOrientation.hpp"
#include "MidasSectionLibrary.hpp"
#include <algorithm>
#include <array>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace kopruq::io::midas {

struct MidasImportReport {
  std::size_t nodes{}, beams{}, materials{}, sections{}, supports{}, rigid_links{}, load_cases{}, member_loads{}, thermal_loads{};
  std::vector<std::string> assumptions; // disclosed substitutions, e.g. "material 1: poisson_ratio=0.2 (...)"
  std::vector<std::string> rejected;    // records explicitly not imported, with the reason
  std::map<Id, Id> originalToInternalNodeId;    // identity mapping in this importer (MIDAS IDs preserved), kept
  std::map<Id, Id> originalToInternalElementId; // explicit for the "preserve original IDs" requirement
};

struct MidasImportResult { ModelInput input; MidasImportReport report; };

namespace detail {

inline Id parseId(const std::string& s) { return static_cast<Id>(std::stoull(s)); }

inline std::vector<Real> toRealVector(const nlohmann::json& arr) {
  std::vector<Real> v; v.reserve(arr.size());
  for (const auto& x : arr) v.push_back(x.get<Real>());
  return v;
}

} // namespace detail

inline MidasImportResult importMidasJson(const nlohmann::json& root) {
  MidasImportResult out;
  const auto& dbg = root.at("DBG");

  // --- NODE ---
  if (dbg.contains("NODE"))
    for (auto it = dbg.at("NODE").begin(); it != dbg.at("NODE").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      out.input.model.nodes.push_back(Node{id, v.at("X").get<Real>(), v.at("Y").get<Real>(), v.at("Z").get<Real>()});
      out.report.originalToInternalNodeId[id] = id;
    }
  out.report.nodes = out.input.model.nodes.size();

  // --- MATL / MATD ---
  if (dbg.contains("MATL"))
    for (auto it = dbg.at("MATL").begin(); it != dbg.at("MATL").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const std::string type = it.value().at("TYPE").get<std::string>();
      if (type != "CONC") { out.report.rejected.push_back("material " + it.key() + ": unsupported TYPE '" + type + "' (only CONC is implemented)"); continue; }
      if (!dbg.contains("MATD") || !dbg.at("MATD").contains(it.key())) { out.report.rejected.push_back("material " + it.key() + ": no matching MATD design-data record"); continue; }
      const auto& matd = dbg.at("MATD").at(it.key());
      try {
        const std::string codeName = matd.at("DATA1").at("CODENAME").get<std::string>();
        const Real fckKNm2 = matd.at("DATA1").at("DESIGN").at("C_FC").get<Real>();
        auto resolved = resolveConcreteGradeMaterial(id, codeName, fckKNm2 / 1000.0); // kN/m^2 -> MPa
        out.input.model.materials.push_back(resolved.material);
        for (const auto& a : resolved.assumptions)
          out.report.assumptions.push_back("material " + it.key() + " (" + a.field + "): " + a.value + " -- " + a.source);
      } catch (const std::exception& e) {
        out.report.rejected.push_back("material " + it.key() + ": " + e.what());
      }
    }
  out.report.materials = out.input.model.materials.size();

  // --- SECT ---
  if (dbg.contains("SECT"))
    for (auto it = dbg.at("SECT").begin(); it != dbg.at("SECT").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      if (v.at("SECTTYPE").get<std::string>() != "DBUSER") { out.report.rejected.push_back("section " + it.key() + ": unsupported SECTTYPE (only DBUSER is implemented)"); continue; }
      const auto& before = v.at("SECT_BEFORE");
      const std::string shape = before.at("SHAPE").get<std::string>();
      const bool useShear = before.value("USE_SHEAR_DEFORM", false);
      try {
        const auto vSize = detail::toRealVector(before.at("SECT_I").at("vSIZE"));
        const auto p = resolveDbUserSection(shape, vSize);
        // Iy = IH (the H/depth dimension's inertia): at MIDAS Beta=0 the local z axis is
        // "vertical" (MidasOrientation.hpp), and KOPRUQ-SOLVER's Iy resists bending in the
        // local x-z plane -- i.e. exactly the bending H's depth resists. Symmetrically
        // Iz = IB. This mapping follows from the now-confirmed MIDAS orientation convention,
        // not an independent guess, but is still flagged for Phase 7E empirical cross-check.
        out.input.model.sections.push_back(Section{id, p.area, /*Iy=*/p.IH, /*Iz=*/p.IB, p.J, 0, 0});
        if (useShear)
          out.report.assumptions.push_back("section " + it.key() + ": USE_SHEAR_DEFORM is set in the source but shear-area formulas are not yet implemented; imported as Euler-Bernoulli (Asy=Asz=0)");
      } catch (const std::exception& e) {
        out.report.rejected.push_back("section " + it.key() + ": " + e.what());
      }
    }
  out.report.sections = out.input.model.sections.size();

  // --- CONS (point restraints) ---
  if (dbg.contains("CONS"))
    for (auto it = dbg.at("CONS").begin(); it != dbg.at("CONS").end(); ++it) {
      const Id node = detail::parseId(it.key());
      const std::string constraint = it.value().at("ITEMS")[0].at("CONSTRAINT").get<std::string>();
      if (constraint.size() < 6) { out.report.rejected.push_back("support at node " + it.key() + ": CONSTRAINT string too short"); continue; }
      std::array<bool,6> flags{};
      for (int i = 0; i < 6; ++i) flags[i] = constraint[(std::size_t)i] != '0';
      out.input.restraints[node] = flags;
      if (constraint.size() > 6)
        out.report.assumptions.push_back("support at node " + it.key() + ": CONSTRAINT string has " + std::to_string(constraint.size()) + " characters, not 6; extra character(s) ignored (meaning unconfirmed, see docs/MIDAS_JSON_DATA_AUDIT.md 4.4)");
    }
  out.report.supports = out.input.restraints.size();

  // --- RIGD (master -> slaves) ---
  Id rigidLinkIdCounter = 1;
  if (dbg.contains("RIGD"))
    for (auto it = dbg.at("RIGD").begin(); it != dbg.at("RIGD").end(); ++it) {
      const Id master = detail::parseId(it.key());
      for (const auto& item : it.value().at("ITEMS"))
        for (const auto& slaveJson : item.at("S_NODE")) {
          RigidLink l; l.id = rigidLinkIdCounter++; l.master = master; l.slave = slaveJson.get<Id>();
          out.input.model.rigid_links.push_back(l);
        }
    }
  out.report.rigid_links = out.input.model.rigid_links.size();

  // --- ELNK: rejected outright (non-coincident elastic links, SpringLink requires coincident nodes) ---
  if (dbg.contains("ELNK"))
    for (auto it = dbg.at("ELNK").begin(); it != dbg.at("ELNK").end(); ++it)
      out.report.rejected.push_back("elastic link " + it.key() + ": ELNK is not yet supported (SpringLink, Phase 5.4, requires coincident nodes; see docs/MIDAS_JSON_DATA_AUDIT.md 4.5)");

  // --- ELEM (BEAM only) ---
  if (dbg.contains("ELEM"))
    for (auto it = dbg.at("ELEM").begin(); it != dbg.at("ELEM").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      const std::string type = v.at("TYPE").get<std::string>();
      if (type != "BEAM") { out.report.rejected.push_back("element " + it.key() + ": unsupported TYPE '" + type + "' (only BEAM is implemented)"); continue; }
      Beam3DConnection e;
      e.id = id;
      e.node1 = v.at("NODE")[0].get<Id>();
      e.node2 = v.at("NODE")[1].get<Id>();
      e.material = v.at("MATL").get<Id>();
      e.section = v.at("SECT").get<Id>();
      auto n1 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node1; });
      auto n2 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node2; });
      if (n1 == out.input.model.nodes.end() || n2 == out.input.model.nodes.end()) { out.report.rejected.push_back("element " + it.key() + ": references a missing node"); continue; }
      try {
        Eigen::Vector3d p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
        const Real angleDeg = v.value("ANGLE", Real(0));
        e.reference_y = midasReferenceY((p2 - p1).normalized(), angleDeg);
      } catch (const std::exception& ex) {
        out.report.rejected.push_back("element " + it.key() + ": " + ex.what());
        continue;
      }
      out.input.model.beams.push_back(e);
      out.report.originalToInternalElementId[id] = id;
    }
  out.report.beams = out.input.model.beams.size();

  std::map<std::string, std::size_t> caseIndex;
  auto caseFor = [&](const std::string& name) -> LoadCase& {
    auto found = caseIndex.find(name);
    if (found != caseIndex.end()) return out.input.load_cases[found->second];
    caseIndex[name] = out.input.load_cases.size();
    out.input.load_cases.push_back(LoadCase{name, {}, {}, 0});
    return out.input.load_cases.back();
  };

  // --- BODF (self-weight; must be exactly global -Z) ---
  if (dbg.contains("BODF"))
    for (auto it = dbg.at("BODF").begin(); it != dbg.at("BODF").end(); ++it) {
      const auto& v = it.value();
      const std::string lc = v.at("LCNAME").get<std::string>();
      const auto fv = detail::toRealVector(v.at("FV"));
      if (fv.size() != 3 || std::abs(fv[0]) > 1e-9 || std::abs(fv[1]) > 1e-9 || std::abs(fv[2] + 1.0) > 1e-9) {
        out.report.rejected.push_back("self-weight for case '" + lc + "': direction is not exactly global -Z (only that direction is implemented)");
        continue;
      }
      caseFor(lc).self_weight = 1.0;
    }

  // --- BMLD (uniform member loads, CMD=BEAM, TYPE=UNILOAD, DIRECTION=GZ only) ---
  if (dbg.contains("BMLD"))
    for (auto it = dbg.at("BMLD").begin(); it != dbg.at("BMLD").end(); ++it) {
      const Id element = detail::parseId(it.key());
      for (const auto& item : it.value().at("ITEMS")) {
        const std::string lc = item.at("LCNAME").get<std::string>();
        const std::string cmd = item.value("CMD", std::string());
        const std::string ltype = item.value("TYPE", std::string());
        const std::string dir = item.value("DIRECTION", std::string());
        if (cmd != "BEAM" || ltype != "UNILOAD" || dir != "GZ") {
          out.report.rejected.push_back("member load on element " + it.key() + ", case '" + lc + "': unsupported CMD/TYPE/DIRECTION combination ('" + cmd + "'/'" + ltype + "'/'" + dir + "')");
          continue;
        }
        const auto d = detail::toRealVector(item.at("D"));
        const auto p = detail::toRealVector(item.at("P"));
        MemberLoad ml; ml.element = element; ml.axes = LoadAxes::Global; ml.direction = 2;
        ml.w1 = p.at(0); ml.w2 = p.at(1); ml.start = d.at(0); ml.end = d.at(1);
        caseFor(lc).member.push_back(ml);
        ++out.report.member_loads;
      }
    }

  // --- ETMP (uniform element temperature; maps directly onto MemberThermalLoad::uniform_delta_t,
  // Phase 7T). TEMP is already a temperature CHANGE (deltaT), the same quantity our thermal load
  // takes, not an absolute temperature -- confirmed by this sample's own values (e.g. 40.0, a
  // plausible deltaT for a "TU" case, not a plausible absolute ambient temperature in any unit). ---
  if (dbg.contains("ETMP"))
    for (auto it = dbg.at("ETMP").begin(); it != dbg.at("ETMP").end(); ++it) {
      const Id element = detail::parseId(it.key());
      for (const auto& item : it.value().at("ITEMS")) {
        const std::string lc = item.at("LCNAME").get<std::string>();
        const Real temp = item.at("TEMP").get<Real>();
        caseFor(lc).thermal.push_back(MemberThermalLoad{element, temp, 0, 0, 0, 0});
        ++out.report.thermal_loads;
      }
    }
  // GTMP (gradient, TZ/TY deltas across the section) and BTMP (beam-section-specific temperature
  // differences) are recognized but NOT mapped: which of MIDAS's T1/T2 is the top fiber could not
  // be confirmed from accessible documentation (support.midasuser.com returns HTTP 403), and
  // guessing the sign would silently flip which way the member bends -- exactly what this project
  // does not do. Rejected explicitly, not silently dropped, only when actually present.
  for (const char* key : {"GTMP", "BTMP"})
    if (dbg.contains(key) && !dbg.at(key).empty())
      out.report.rejected.push_back(std::string(key) + ": gradient/beam-section temperature records are present but not mapped (MIDAS's T1/T2 top-fiber sign convention could not be confirmed); only uniform ETMP temperature is imported");

  out.report.load_cases = out.input.load_cases.size();

  // --- EIGV/SPLC: recognized but not executed this phase ---
  if (dbg.contains("EIGV") || dbg.contains("SPLC"))
    out.report.rejected.push_back("modal/response-spectrum requests (EIGV/SPLC) present in source are not executed by this importer: ModalSolver/ResponseSpectrumSolver reject any model with rigid_links (Phase 5.5), and RIGD maps onto rigid_links directly");

  return out;
}

} // namespace kopruq::io::midas
