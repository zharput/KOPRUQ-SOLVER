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
// (self-weight, direction must be exactly global -Z), CNLD (concentrated
// nodal loads FX/FY/FZ/MX/MY/MZ, global axes), BMLD
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
// Timoshenko shear areas, when SECT_BEFORE's USE_SHEAR_DEFORM is true, are
// derived from each shape's classical shear correction factor
// (MidasSectionLibrary.hpp: 5/6 for SB rectangles, 0.9 for SR circles, a
// coarser web/flange-area approximation for B hollow boxes) -- not read
// from MIDAS directly (MIDAS exports no effective shear area for a DBUSER
// section), and this substitution is recorded, not hidden. Sections with
// USE_SHEAR_DEFORM false still import as Euler-Bernoulli (Asy=Asz=0).
//
// NOT executed in this phase (recorded in report, not silently dropped):
// EIGV (modal analysis request -- number of modes only, `iFREQ`) IS now read, confirmed from a
// real MIDAS NX JSON export (`verification/models/13 Eigen-1`, 2026-11-17) -- see the EIGV
// handling below for the exact field shape. ModalSolver/ResponseSpectrumSolver support
// `rigid_links` (Phase 7H, 2026-11-17: an exact Guyan/DOF-elimination extension of the same
// RigidLinkReduction LinearStaticSolver already used, independently verified in
// tests/unit/rigid_link_modal_tests.cpp), and the CLI's `modal`/`spectrum` commands dispatch to
// those solvers regardless of `--format`, so a RIGD-containing MIDAS model can already run modal
// analysis today via `modal <file> --format midas-nx-json`.
// SPLC/SPFC (response-spectrum requests) ARE now translated into `response_spectra` entries,
// confirmed from a real MIDAS NX JSON export (`verification/models/16 Spektrum-1`, 2026-11-17)
// -- see the SPLC/SPFC handling below for the exact field shapes. Explicitly NOT translated, and
// explicitly rejected (not silently approximated) when present: SPLC `ANGLE` != 0 (an arbitrary-
// angle spectrum direction, not an axis-aligned one -- `ResponseSpectrumInput::direction` only
// accepts global X/Y/Z); any `aUSEMODE` entry with `bUSE=false` or `MSFACTOR != 1.0` (per-mode
// exclusion/scaling, which `ResponseSpectrumInput` has no way to express); any `SPLC.COMTYPE`
// other than "CQC" or "SRSS". `SPLC.CQCRATIO`'s exact meaning was not confirmed from accessible
// documentation (possibly a closely-spaced-mode cutoff ratio in some codes' CQC variants, not a
// damping ratio) and is NOT used -- the CQC damping ratio instead comes from the referenced
// SPFC's own `DRATIO`, which is unambiguous.
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
  std::size_t nodal_loads{}; // CNLD records imported (concentrated nodal forces/moments)
  std::size_t elastic_links_rejected{}; // ELNK records present but rejected outright (not a count of anything imported)
  std::vector<std::string> assumptions; // disclosed substitutions, e.g. "material 1: poisson_ratio=0.2 (...)"
  std::vector<std::string> rejected;    // records explicitly not imported, with the reason
  std::map<Id, Id> originalToInternalNodeId;    // identity mapping in this importer (MIDAS IDs preserved), kept
  std::map<Id, Id> originalToInternalElementId; // explicit for the "preserve original IDs" requirement
  // Node coordinate bounding box, purely informational (e.g. for a GUI's model-summary display)
  // -- not used anywhere in the solve itself. Left at zero/false if there are no nodes at all.
  bool hasBoundingBox{false};
  Real minX{}, maxX{}, minY{}, maxY{}, minZ{}, maxZ{};
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
      const Real x = v.at("X").get<Real>(), y = v.at("Y").get<Real>(), z = v.at("Z").get<Real>();
      out.input.model.nodes.push_back(Node{id, x, y, z});
      out.report.originalToInternalNodeId[id] = id;
      if (!out.report.hasBoundingBox) {
        out.report.minX = out.report.maxX = x;
        out.report.minY = out.report.maxY = y;
        out.report.minZ = out.report.maxZ = z;
        out.report.hasBoundingBox = true;
      } else {
        out.report.minX = std::min(out.report.minX, x); out.report.maxX = std::max(out.report.maxX, x);
        out.report.minY = std::min(out.report.minY, y); out.report.maxY = std::max(out.report.maxY, y);
        out.report.minZ = std::min(out.report.minZ, z); out.report.maxZ = std::max(out.report.maxZ, z);
      }
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
        const std::string gradeName = matd.at("DATA1").value("CODEMATLNAME", std::string());
        const Real fckKNm2 = matd.at("DATA1").at("DESIGN").at("C_FC").get<Real>();
        auto resolved = resolveConcreteGradeMaterial(id, codeName, fckKNm2 / 1000.0, gradeName); // kN/m^2 -> MPa
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
        // Asy (y-bending plane, uses Iz=IB per Section.hpp) <- AsB; Asz (z-bending plane,
        // uses Iy=IH) <- AsH -- same IH/IB <-> Iy/Iz correspondence as above, applied to the
        // companion shear areas. Asy=Asz=0 (Euler-Bernoulli) when USE_SHEAR_DEFORM is false,
        // matching MIDAS's own per-section flag rather than always applying shear deformation.
        const Real Asy = useShear ? p.AsB : Real(0);
        const Real Asz = useShear ? p.AsH : Real(0);
        out.input.model.sections.push_back(Section{id, p.area, /*Iy=*/p.IH, /*Iz=*/p.IB, p.J, Asy, Asz});
        if (useShear)
          out.report.assumptions.push_back("section " + it.key() + ": USE_SHEAR_DEFORM is set in the source; shear areas computed from the shape's classical Timoshenko shear correction factor (see MidasSectionLibrary.hpp), not read from MIDAS directly (MIDAS does not export an effective shear area for a DBUSER section)");
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
    for (auto it = dbg.at("ELNK").begin(); it != dbg.at("ELNK").end(); ++it) {
      out.report.rejected.push_back("elastic link " + it.key() + ": ELNK is not yet supported (SpringLink, Phase 5.4, requires coincident nodes; see docs/MIDAS_JSON_DATA_AUDIT.md 4.5)");
      ++out.report.elastic_links_rejected;
    }

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

  // --- CNLD (concentrated nodal loads: FX/FY/FZ/MX/MY/MZ, global axes -- MIDAS does not offer a
  // local-axis option for nodal loads, unlike BMLD's member loads) ---
  if (dbg.contains("CNLD"))
    for (auto it = dbg.at("CNLD").begin(); it != dbg.at("CNLD").end(); ++it) {
      const Id node = detail::parseId(it.key());
      for (const auto& item : it.value().at("ITEMS")) {
        const std::string lc = item.at("LCNAME").get<std::string>();
        NodalLoad nl; nl.node = node;
        nl.values(0) = item.value("FX", Real(0));
        nl.values(1) = item.value("FY", Real(0));
        nl.values(2) = item.value("FZ", Real(0));
        nl.values(3) = item.value("MX", Real(0));
        nl.values(4) = item.value("MY", Real(0));
        nl.values(5) = item.value("MZ", Real(0));
        caseFor(lc).nodal.push_back(nl);
        ++out.report.nodal_loads;
      }
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

  // --- EIGV: modal analysis request (number of modes only) ---
  // Real field shape confirmed from an actual MIDAS NX JSON export (verification/models/13
  // Eigen-1/13 Eigen-1.json, 2026-11-17): {"EIGV":{"<id>":{"TYPE":"LANCZOS","iFREQ":20,
  // "bMINMAX":false,"bSTRUM":false}}}. Only `iFREQ` (number of modes) is read -- `TYPE` (solver
  // algorithm choice), `bMINMAX`/`bSTRUM` (Lanczos-specific options) have no counterpart in
  // ModalSolver (a single, already-independently-verified dense eigensolve, see Phase 6.1) and
  // are not translated; if more than one EIGV entry is present, the first (by key) is used, since
  // MIDAS NX only ever exports one modal control record in every sample seen so far.
  if (dbg.contains("EIGV") && !dbg.at("EIGV").empty()) {
    const auto& first = dbg.at("EIGV").begin().value();
    if (first.contains("iFREQ") && first.at("iFREQ").is_number_integer()) {
      out.input.modal_request.present = true;
      out.input.modal_request.num_modes = first.at("iFREQ").get<int>();
      out.report.assumptions.push_back("EIGV: modal analysis requested, " + std::to_string(out.input.modal_request.num_modes) +
          " modes (iFREQ) -- solver algorithm/options (TYPE/bMINMAX/bSTRUM) are not translated, ModalSolver always uses its own single, independently-verified dense eigensolve");
    } else {
      out.report.rejected.push_back("EIGV: present but 'iFREQ' field missing or not an integer -- modal analysis request not read");
    }
  }

  // --- SPLC/SPFC: response-spectrum requests ---
  // Real field shapes confirmed from an actual MIDAS NX JSON export
  // (verification/models/16 Spektrum-1/16 Spektrum-1.json, 2026-11-17). SPLC references its
  // spectrum function by NAME (not by key/id), so SPFC entries are indexed by name first.
  if (dbg.contains("SPLC") && !dbg.at("SPLC").empty()) {
    std::map<std::string, const nlohmann::json*> spfcByName;
    if (dbg.contains("SPFC"))
      for (auto it = dbg.at("SPFC").begin(); it != dbg.at("SPFC").end(); ++it)
        if (it.value().contains("NAME")) spfcByName[it.value().at("NAME").get<std::string>()] = &it.value();

    for (auto it = dbg.at("SPLC").begin(); it != dbg.at("SPLC").end(); ++it) {
      const auto& splc = it.value();
      const std::string caseName = splc.value("NAME", std::string("SPLC") + it.key());
      bool ok = true;
      std::string reason;

      // Direction: only a single axis-aligned, zero-angle direction is supported -- SPLC's own
      // ANGLE lets a spectrum be applied at an arbitrary horizontal angle, which
      // ResponseSpectrumInput::direction (global X/Y/Z only) cannot express.
      int direction = -1;
      const double angle = splc.value("ANGLE", 0.0);
      const std::string dir = splc.value("DIR", std::string());
      if (angle != 0.0) { ok = false; reason = "ANGLE != 0 (an arbitrary-angle spectrum direction) is not supported"; }
      else if (!dir.empty() && (dir[0] == 'X' || dir[0] == 'x')) direction = 0;
      else if (!dir.empty() && (dir[0] == 'Y' || dir[0] == 'y')) direction = 1;
      else if (!dir.empty() && (dir[0] == 'Z' || dir[0] == 'z')) direction = 2;
      else { ok = false; reason = "DIR '" + dir + "' not recognized (expected to start with X/Y/Z)"; }
      if (ok && dir.size() > 1)
        out.report.assumptions.push_back("SPLC " + caseName + ": DIR='" + dir + "' with ANGLE=0 -- direction taken as its first letter ('" +
            std::string(1, dir[0]) + "'); nothing past the first letter of a compound DIR is otherwise interpreted");

      // Modal combination: only CQC and SRSS are implemented (Phase 7I.1, 2026-11-17).
      ModalCombination combo = ModalCombination::SRSS;
      const std::string comtype = splc.value("COMTYPE", std::string("SRSS"));
      if (comtype == "CQC") combo = ModalCombination::CQC;
      else if (comtype == "SRSS") combo = ModalCombination::SRSS;
      else if (ok) { ok = false; reason = "COMTYPE '" + comtype + "' is neither CQC nor SRSS"; }

      // Per-mode use/scale: only the trivial case (every mode used, every factor 1.0) is
      // supported -- ResponseSpectrumInput has no way to exclude or rescale one mode's
      // contribution.
      if (ok && splc.contains("aUSEMODE"))
        for (const auto& um : splc.at("aUSEMODE"))
          if (!um.value("bUSE", true) || um.value("MSFACTOR", 1.0) != 1.0) {
            ok = false; reason = "aUSEMODE has a per-mode exclusion or scale factor, which ResponseSpectrumInput cannot express"; break;
          }

      ResponseSpectrumInput input;
      Real dampingRatio = 0, gravUsed = 0;
      std::string funcNameUsed;
      if (ok) {
        if (!splc.contains("aFUNCNAME") || splc.at("aFUNCNAME").empty()) { ok = false; reason = "aFUNCNAME is missing or empty"; }
        else {
          funcNameUsed = splc.at("aFUNCNAME").front().get<std::string>();
          auto spfcIt = spfcByName.find(funcNameUsed);
          if (spfcIt == spfcByName.end()) { ok = false; reason = "referenced SPFC '" + funcNameUsed + "' not found"; }
          else {
            const auto& spfc = *spfcIt->second;
            gravUsed = spfc.value("GRAV", 9.806);
            dampingRatio = spfc.value("DRATIO", 0.05);
            if (!spfc.contains("aFUNC") || spfc.at("aFUNC").size() < 2) { ok = false; reason = "SPFC '" + funcNameUsed + "' has fewer than 2 aFUNC points"; }
            else {
              // MIDAS's own spectrum table is in units of g (confirmed by cross-checking its
              // T=0/plateau values against the SPFC's own Eurocode 8 parameters: ag=0.2,
              // S=1.15, I=1.2 -> ag*S*I=0.276, exactly the T=0 aFUNC value) -- converted to
              // m/s^2 (ResponseSpectrumInput's own documented unit) using THIS SPFC's own GRAV,
              // never a hardcoded constant, so the conversion matches whatever MIDAS itself used.
              for (const auto& pt : spfc.at("aFUNC")) {
                // interpolateSpectrum (ResponseSpectrumSolver.hpp) requires a strictly positive,
                // strictly increasing period -- MIDAS's own T=0 anchor point is nudged to a tiny
                // positive value (1e-6 s) rather than dropped, since many real high-frequency
                // modes (confirmed on model 16 itself: 16/20 modes have a period shorter than the
                // smallest other tabulated period, 0.04 s) would otherwise fall outside the
                // table's range entirely. This keeps the real T=0 VALUE, it does not invent or
                // extrapolate a new one.
                const double period = std::max(pt.value("PERIOD", 0.0), 1e-6);
                const double value = pt.value("VALUE", 0.0);
                input.spectrum.push_back({period, value * gravUsed});
              }
              if (input.spectrum.size() < 2) { ok = false; reason = "SPFC '" + funcNameUsed + "' has fewer than 2 usable (period>0) points"; }
            }
          }
        }
      }

      if (!ok) {
        out.report.rejected.push_back("SPLC " + caseName + ": not translated into a response_spectra entry -- " + reason);
        continue;
      }

      input.direction = direction;
      input.num_modes = out.input.modal_request.present ? out.input.modal_request.num_modes : 6;
      input.combination = combo;
      input.damping_ratio = dampingRatio;
      out.input.response_spectra.push_back({caseName, input});
      std::string note = "SPLC " + caseName + ": response spectrum imported from SPFC '" + funcNameUsed + "' (direction=" +
          std::string(1, "XYZ"[direction]) + ", " + comtype + " combination";
      if (combo == ModalCombination::CQC) note += ", damping ratio " + std::to_string(dampingRatio) + " from SPFC DRATIO -- SPLC's own CQCRATIO is not used, its exact meaning is unconfirmed";
      note += "); values converted from g to m/s^2 using this SPFC's own GRAV (" + std::to_string(gravUsed) + ")";
      out.report.assumptions.push_back(note);
    }
  } else if (dbg.contains("SPFC") && !dbg.at("SPFC").empty()) {
    out.report.rejected.push_back("SPFC (response-spectrum function) present without any SPLC (response-spectrum case) referencing it -- nothing to import");
  }

  return out;
}

} // namespace kopruq::io::midas
