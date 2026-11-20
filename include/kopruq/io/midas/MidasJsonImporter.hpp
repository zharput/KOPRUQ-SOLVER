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
// temperature CHANGE, the same quantity that load takes). ELNK with
// LINK:"GEN" and every R_S entry false -> SpringConnection (Phase 5.4
// revision, 2026-11-18, see the ELNK handling below for exactly what is
// and is not used from each record, and why). NSPR with TYPE="LINEAR" and
// every F_S/DAMPING/Cr/EFFAREA/DK at its default value -> JointSpringConnection
// (Phase 5.4b, 2026-11-18). FRLS with bVALUE=false -> Beam3DConnection::releases
// (per-DOF end releases / "mafsal", 2026-11-18).
//
// Explicitly REJECTED, not silently ignored or approximated (recorded in
// MidasImportReport::rejected with the record identified): any ELEM TYPE
// other than BEAM; any MATL TYPE other than CONC or any code family other
// than EN..(RC); any SECT shape other than SB/B/SR; any ELNK whose LINK is
// not "GEN" or whose R_S has any true entry (see the ELNK handling below);
// BTMP (beam-section temperature profile -- conventions not verified);
// GTMP items other than TYPE 1 (beam), or without a usable gradient depth
// (GTMP beam gradients themselves ARE imported since 2026-11-20, sign
// convention proven from MIDAS's manual and its own model-22 results, see
// the GTMP handling below); PSC sections other than a symmetric 1CEL with
// every joint off; STYP values other than 0 (3D) and 1 (X-Z plane);
// section offsets other than OFFSET_PT "CC"/"CT" without user-defined parts
// ("CT" is imported as a rigid node-to-centroid member-end offset);
// any BODF whose direction is not exactly global -Z; any BMLD
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
// explicitly rejected (not silently approximated) when present: SPLC `ANGLE` not a multiple of
// 90 degrees within the plane its `DIR` names (an arbitrary-angle spectrum direction --
// `ResponseSpectrumInput::direction` only accepts global X/Y/Z; ANGLE=0/90/180/270 resolve to
// DIR's two named axes, confirmed against two real exports, models 16 and 17 -- see the SPLC
// handling below); any `aUSEMODE` entry with `bUSE=false` or `MSFACTOR != 1.0` (per-mode
// exclusion/scaling, which `ResponseSpectrumInput` has no way to express); any `SPLC.COMTYPE`
// other than "CQC" or "SRSS". `SPLC.CQCRATIO`'s exact meaning was not confirmed from accessible
// documentation (possibly a closely-spaced-mode cutoff ratio in some codes' CQC variants, not a
// damping ratio) and is NOT used -- the CQC damping ratio instead comes from the referenced
// SPFC's own `DRATIO`, which is unambiguous.
#include "../ModelJson.hpp"
#include "../../elements/Truss3D.hpp"
#include "MidasMaterialLibrary.hpp"
#include "MidasOrientation.hpp"
#include "MidasSectionLibrary.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace kopruq::io::midas {

struct MidasImportReport {
  std::size_t nodes{}, beams{}, materials{}, sections{}, supports{}, rigid_links{}, load_cases{}, member_loads{}, thermal_loads{};
  std::size_t nodal_loads{}; // CNLD records imported (concentrated nodal forces/moments)
  std::size_t springs{}; // ELNK (LINK:"GEN") records imported as SpringConnection
  std::size_t joint_springs{}; // NSPR records imported as JointSpringConnection
  std::size_t elastic_links_rejected{}; // ELNK records present but NOT imported (unsupported LINK type, unconfirmed R_S release, missing node, or bad SDR) -- not a count of `springs`
  std::size_t combinations{}; // LCOM-GEN records imported as LoadCombination (nested CB references expanded to pure load-case terms)
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
      if (type != "CONC" && type != "STEEL") { out.report.rejected.push_back("material " + it.key() + ": unsupported TYPE '" + type + "' (only CONC, STEEL are implemented)"); continue; }
      try {
        ResolvedMaterial resolved = [&]() {
          if (type == "CONC") {
            if (!dbg.contains("MATD") || !dbg.at("MATD").contains(it.key())) throw std::invalid_argument("no matching MATD design-data record");
            const auto& matd = dbg.at("MATD").at(it.key());
            const std::string codeName = matd.at("DATA1").at("CODENAME").get<std::string>();
            const std::string gradeName = matd.at("DATA1").value("CODEMATLNAME", std::string());
            return resolveConcreteGradeMaterial(id, codeName, matd.at("DATA1").at("DESIGN").at("C_FC").get<Real>() / 1000.0, gradeName); // kN/m^2 -> MPa
          }
          // STEEL: unlike CONC, the design-code family and grade live directly on MATL's own
          // PARAM[0] (STANDARD, DB) -- there is no separate MATD record (confirmed empirically,
          // model 20: MATD is present in the export but empty for these material ids).
          if (!it.value().contains("PARAM") || it.value().at("PARAM").empty()) throw std::invalid_argument("no PARAM entry (design code/grade)");
          const auto& param = it.value().at("PARAM")[0];
          const std::string codeName = param.at("STANDARD").get<std::string>();
          const std::string gradeName = param.value("DB", it.value().value("NAME", std::string()));
          return resolveSteelGradeMaterial(id, codeName, gradeName);
        }();
        out.input.model.materials.push_back(resolved.material);
        for (const auto& a : resolved.assumptions)
          out.report.assumptions.push_back("material " + it.key() + " (" + a.field + "): " + a.value + " -- " + a.source);
      } catch (const std::exception& e) {
        out.report.rejected.push_back("material " + it.key() + ": " + e.what());
      }
    }
  out.report.materials = out.input.model.materials.size();

  // sectionExtent: section id -> (overall H, overall B), for temperature gradients with MIDAS's
  // "Use Section Hz / Hy" (see the GTMP handling below). Absent or 0 = not defined.
  std::map<Id, std::pair<Real, Real>> sectionExtent;
  // sectionOffsetDown: section id -> distance the member axis (centroid) lies BELOW the node, along
  // the member's local -z, for a MIDAS "Center-Top" section offset (see the SECT handling).
  std::map<Id, Real> sectionOffsetDown;
  // sectionAxialOnly: sections valid only for TRUSS members (user angle "L": MIDAS's beam-bending
  // orientation of an angle is unverified, see MidasSectionLibrary.hpp angleUserSection).
  std::set<Id> sectionAxialOnly;

  // --- SECT ---
  if (dbg.contains("SECT"))
    for (auto it = dbg.at("SECT").begin(); it != dbg.at("SECT").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      const std::string sectType = v.at("SECTTYPE").get<std::string>();
      if (sectType != "DBUSER" && sectType != "PSC") { out.report.rejected.push_back("section " + it.key() + ": unsupported SECTTYPE '" + sectType + "' (only DBUSER and PSC are implemented)"); continue; }
      const auto& before = v.at("SECT_BEFORE");
      const std::string shape = before.at("SHAPE").get<std::string>();
      const bool useShear = before.value("USE_SHEAR_DEFORM", false);
      try {
        const auto& sectI = before.at("SECT_I");
        ResolvedSectionProperties p;
        if (sectType == "PSC") {
          // PSC: only the single-cell symmetric box with every joint off (MidasSectionLibrary.hpp,
          // psc1CellSection); anything else is rejected, not approximated.
          if (shape != "1CEL") throw std::invalid_argument("unsupported PSC shape '" + shape + "' (only 1CEL is implemented)");
          if (!before.value("USE_SYMMETRIC", true)) throw std::invalid_argument("PSC 1CEL with USE_SYMMETRIC=false is not supported");
          if (before.value("USE_USER_INTPUT_STIFF", false)) throw std::invalid_argument("PSC with user-input stiffness (USE_USER_INTPUT_STIFF) is not supported");
          if (before.value("USE_SMALL_HOLE", false)) throw std::invalid_argument("PSC with USE_SMALL_HOLE is not supported");
          if (before.contains("JOINT"))
            for (const auto& j : before.at("JOINT"))
              if (j.get<bool>()) throw std::invalid_argument("PSC 1CEL with a joint (JO1..JO3/JI1..JI5) switched on is not supported");
          const auto A = detail::toRealVector(sectI.at("vSIZE_PSC_A")), B = detail::toRealVector(sectI.at("vSIZE_PSC_B"));
          const auto C = detail::toRealVector(sectI.at("vSIZE_PSC_C")), D = detail::toRealVector(sectI.at("vSIZE_PSC_D"));
          if (A.size() < 6 || B.size() < 6 || C.size() < 10 || D.size() < 5) throw std::invalid_argument("PSC 1CEL: vSIZE_PSC_A/B/C/D arrays are too short");
          p = psc1CellSection(Psc1CellDims{A[0], A[1], A[4], B[0], B[3], B[5], C[0], C[1], C[4], C[6], C[9], D[0], D[4]});
          out.report.assumptions.push_back("section " + it.key() + ": PSC 1CEL J and shear areas from KOPRUQ's converged 2D FE section analysis (SectionFE.hpp); MIDAS evaluates them on its own coarser section mesh, so they can differ from MIDAS's by a fraction of a percent");
        } else {
          // Catalog / database profiles (DB_NAME: AISC, EN/UNI, ...) are not supported -- removed
          // 2026-11-20 by user decision (precast concrete bridges first; steel members are entered
          // as user-defined I/H or T sections). Rejected explicitly, never approximated.
          if (sectI.contains("DB_NAME"))
            throw std::invalid_argument("catalog/database section (DB_NAME '" + sectI.at("DB_NAME").get<std::string>() + "', SECT_NAME '" +
                                        sectI.value("SECT_NAME", std::string()) + "') is not supported; define it as a user (DB/User 'User') section");
          p = resolveDbUserSection(shape, detail::toRealVector(sectI.at("vSIZE")));
        }
        // Section offset (OFFSET_PT; 2026-11-20). MIDAS places the node at the named point of the
        // section ("CC" centre-centre = centroid, "CT" centre-top = top fiber above the centroid)
        // and the member axis stays at the centroid -- a rigid offset from node to member end.
        // Confirmed for "CT" from MIDAS's own results of models 02/03/04/08 (axial DX at the
        // top-fiber nodes, torsion RX under a horizontal load); every other offset point and any
        // user-defined offset (OFFSET_CENTER, USER_OFFSET_REF, HORZ/VERT_OFFSET_OPT,
        // USERDEF_OFFSET_YI/ZI) is unverified and rejected rather than guessed.
        {
          const std::string offPt = before.value("OFFSET_PT", std::string("CC"));
          const bool userParts = before.value("OFFSET_CENTER", 0) != 0 || before.value("USER_OFFSET_REF", 0) != 0 ||
                                 before.value("HORZ_OFFSET_OPT", 0) != 0 || before.value("VERT_OFFSET_OPT", 0) != 0 ||
                                 before.value("USERDEF_OFFSET_YI", Real(0)) != 0 || before.value("USERDEF_OFFSET_ZI", Real(0)) != 0;
          if (userParts) throw std::invalid_argument("user-defined section offset (OFFSET_CENTER/USER_OFFSET_REF/HORZ_OFFSET_OPT/VERT_OFFSET_OPT/USERDEF_OFFSET_*) is not supported");
          if (offPt == "CT") {
            if (!(p.topFiber > 0)) throw std::invalid_argument("section offset 'CT' needs a defined top-fiber distance (not available for this shape)");
            sectionOffsetDown[id] = p.topFiber;
          } else if (offPt != "CC") {
            throw std::invalid_argument("section offset point '" + offPt + "' is not supported (only CC and CT are verified)");
          }
        }
        if (p.axialOnly) sectionAxialOnly.insert(id);
        if (p.extentH > 0 && p.extentB > 0) sectionExtent[id] = {p.extentH, p.extentB};
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

  // --- ELNK: "GEN" (General) elastic links -> SpringConnection (Phase 5.4 revision, 2026-11-18:
  // SpringLink no longer requires coincident nodes -- see docs/MIDAS_JSON_DATA_AUDIT.md 4.5).
  // Orientation reuses the exact same node1->node2 + "ANGLE" Beta-angle convention already
  // confirmed for ELEM/BEAM (midasReferenceY, above) rather than inventing a separate rule --
  // MIDAS's Element Coordinate System convention applies to its line elements generally, this is
  // not assumed independently per element type. Only LINK:"GEN" is implemented (the only type
  // seen in any real export available to this project, verification/models/07); any other LINK
  // type is rejected, not silently treated as GEN. `R_S` (per-DOF release-like flags) are only
  // confirmed safe to ignore when every one of the 6 is false (the only value seen in real data);
  // any true entry is rejected rather than guessed. `DR` is read but deliberately NOT used:
  // SpringLink's stiffness directly couples node1's and node2's own DOFs with no interior point,
  // so there is no formulation concept for DR to feed into -- if it represents some MIDAS-internal
  // offset/insertion-point behavior this import does not reproduce, that would only be caught by
  // comparing results against the model's own real MIDAS reference export (not assumed benign).
  if (dbg.contains("ELNK"))
    for (auto it = dbg.at("ELNK").begin(); it != dbg.at("ELNK").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      const std::string linkType = v.value("LINK", std::string());
      if (linkType != "GEN") { out.report.rejected.push_back("elastic link " + it.key() + ": unsupported LINK type '" + linkType + "' (only GEN is implemented)"); ++out.report.elastic_links_rejected; continue; }
      if (v.contains("R_S")) {
        bool anyReleased = false;
        for (const auto& r : v.at("R_S")) if (r.get<bool>()) anyReleased = true;
        if (anyReleased) { out.report.rejected.push_back("elastic link " + it.key() + ": R_S (release flags) has a true entry, meaning not confirmed -- not implemented"); ++out.report.elastic_links_rejected; continue; }
      }
      SpringConnection e;
      e.id = id;
      e.node1 = v.at("NODE")[0].get<Id>();
      e.node2 = v.at("NODE")[1].get<Id>();
      auto n1 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node1; });
      auto n2 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node2; });
      if (n1 == out.input.model.nodes.end() || n2 == out.input.model.nodes.end()) { out.report.rejected.push_back("elastic link " + it.key() + ": references a missing node"); ++out.report.elastic_links_rejected; continue; }
      const auto sdr = detail::toRealVector(v.at("SDR"));
      if (sdr.size() != 6) { out.report.rejected.push_back("elastic link " + it.key() + ": SDR must have 6 entries (kx,ky,kz,krx,kry,krz)"); ++out.report.elastic_links_rejected; continue; }
      for (int i = 0; i < 6; ++i) e.stiffness[(std::size_t)i] = sdr[(std::size_t)i];
      try {
        Eigen::Vector3d p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
        const Real angleDeg = v.value("ANGLE", Real(0));
        if ((p2 - p1).norm() > 1e-9) {
          e.local_x = (p2 - p1).normalized();
          e.reference_y = midasReferenceY(e.local_x, angleDeg);
        } // else: coincident nodes, keep SpringConnection's own default (1,0,0)/(0,1,0) -- no
          // direction to derive, exactly as for a bearing/isolator (see SpringLink.hpp).
      } catch (const std::exception& ex) {
        out.report.rejected.push_back("elastic link " + it.key() + ": " + ex.what()); ++out.report.elastic_links_rejected; continue;
      }
      out.input.model.springs.push_back(e);
    }
  out.report.springs = out.input.model.springs.size();

  // --- NSPR: single-node elastic supports ("Nodal Spring") -> JointSpringConnection (Phase
  // 5.4b, 2026-11-18). Keyed by node id (not its own id space), each with an ITEMS[0] holding
  // the actual record -- real field shapes confirmed from verification/models/19's own JSON.
  // Only TYPE="LINEAR" (every record seen in real data) is implemented; nonlinear spring types
  // are rejected, not approximated as linear. SDR maps directly to local kx,ky,kz,krx,kry,krz
  // (same layout as ELNK's SDR). Orientation: NSPR carries no local_x/reference_y/ANGLE field at
  // all in any real export seen -- a single-node spring with no second node and no beta angle has
  // no MIDAS-documented orientation convention available, so it is imported with
  // JointSpringConnection's own default axes (global X/Y, i.e. local axes = global axes), the
  // same default `springs`/`joint_springs` already use when nothing else is specified. F_S (per-
  // DOF release-like flags), DAMPING, Cr, EFFAREA and DK are only confirmed safe to ignore when
  // every one is false/zero (the only values seen in real data); any non-default value in any of
  // them is rejected rather than guessed.
  if (dbg.contains("NSPR")) {
    Id jointSpringIdCounter = 1;
    for (auto it = dbg.at("NSPR").begin(); it != dbg.at("NSPR").end(); ++it) {
      const Id node = detail::parseId(it.key());
      if (!it.value().contains("ITEMS") || it.value().at("ITEMS").empty()) { out.report.rejected.push_back("joint spring at node " + it.key() + ": no ITEMS entry"); continue; }
      const auto& item = it.value().at("ITEMS")[0];
      const std::string type = item.value("TYPE", std::string());
      if (type != "LINEAR") { out.report.rejected.push_back("joint spring at node " + it.key() + ": unsupported TYPE '" + type + "' (only LINEAR is implemented)"); continue; }
      if (item.value("DAMPING", false)) { out.report.rejected.push_back("joint spring at node " + it.key() + ": DAMPING=true is not implemented"); continue; }
      bool nonDefault = false;
      if (item.contains("Cr")) for (const auto& c : item.at("Cr")) if (c.get<Real>() != 0) nonDefault = true;
      if (item.value("EFFAREA", Real(0)) != 0) nonDefault = true;
      if (item.contains("DK")) for (const auto& d : item.at("DK")) if (d.get<Real>() != 0) nonDefault = true;
      if (nonDefault) { out.report.rejected.push_back("joint spring at node " + it.key() + ": Cr/EFFAREA/DK has a non-default value, meaning not confirmed -- not implemented"); continue; }
      if (item.contains("F_S")) {
        bool anyReleased = false;
        for (const auto& f : item.at("F_S")) if (f.get<bool>()) anyReleased = true;
        if (anyReleased) { out.report.rejected.push_back("joint spring at node " + it.key() + ": F_S (release flags) has a true entry, meaning not confirmed -- not implemented"); continue; }
      }
      auto found = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == node; });
      if (found == out.input.model.nodes.end()) { out.report.rejected.push_back("joint spring at node " + it.key() + ": references a missing node"); continue; }
      if (!item.contains("SDR") || item.at("SDR").size() != 6) { out.report.rejected.push_back("joint spring at node " + it.key() + ": SDR must have 6 entries (kx,ky,kz,krx,kry,krz)"); continue; }
      JointSpringConnection e; e.id = jointSpringIdCounter++; e.node = node;
      const auto sdr = detail::toRealVector(item.at("SDR"));
      for (int i = 0; i < 6; ++i) e.stiffness[(std::size_t)i] = sdr[(std::size_t)i];
      out.input.model.joint_springs.push_back(e);
    }
  }
  out.report.joint_springs = out.input.model.joint_springs.size();

  // Nodes touched by a TRUSS element vs. a BEAM element -- used below to auto-restrain the
  // rotational DOFs (RX/RY/RZ) of any node connected ONLY by trusses, since such a node has no
  // physical rotational stiffness source at all (Truss3D.hpp's own header: "a node connected
  // only by trusses still has no rotational stiffness source and must have its RX/RY/RZ
  // restrained"), matching MIDAS's own implicit convention for a pin-jointed truss boundary
  // condition (MIDAS does not export an explicit restraint for this -- it is a disclosed,
  // reported assumption, not a silent one).
  std::set<Id> trussNodes, beamNodes;

  // --- ELEM (BEAM only) ---
  if (dbg.contains("ELEM"))
    for (auto it = dbg.at("ELEM").begin(); it != dbg.at("ELEM").end(); ++it) {
      const Id id = detail::parseId(it.key());
      const auto& v = it.value();
      const std::string type = v.at("TYPE").get<std::string>();
      if (type != "BEAM" && type != "TRUSS") { out.report.rejected.push_back("element " + it.key() + ": unsupported TYPE '" + type + "' (only BEAM, TRUSS are implemented)"); continue; }
      Beam3DConnection e;
      e.id = id;
      e.node1 = v.at("NODE")[0].get<Id>();
      e.node2 = v.at("NODE")[1].get<Id>();
      e.material = v.at("MATL").get<Id>();
      e.section = v.at("SECT").get<Id>();
      auto n1 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node1; });
      auto n2 = std::find_if(out.input.model.nodes.begin(), out.input.model.nodes.end(), [&](const Node& n){ return n.id == e.node2; });
      if (n1 == out.input.model.nodes.end() || n2 == out.input.model.nodes.end()) { out.report.rejected.push_back("element " + it.key() + ": references a missing node"); continue; }
      // TRUSS: a pin-ended member, same stiffness/mass pipeline as BEAM but with every rotational
      // DOF released (see Truss3D.hpp) -- MIDAS's own TRUSS type carries no end moment, exactly
      // this release pattern, so FRLS (an explicit hinge record) is neither expected nor consulted
      // for a TRUSS element below; reference_y is immaterial once every rotation is released, so
      // Truss3D's own trussReference() is used instead of MIDAS's ANGLE-based orientation.
      if (type == "TRUSS") {
        if (sectionOffsetDown.count(e.section)) { out.report.rejected.push_back("element " + it.key() + ": TRUSS with a section offset is not supported"); continue; }
        Eigen::Vector3d p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
        e.reference_y = trussReference(p2 - p1);
        e.releases = trussReleases();
        out.input.model.beams.push_back(e);
        out.report.originalToInternalElementId[id] = id;
        trussNodes.insert(e.node1); trussNodes.insert(e.node2);
        continue;
      }
      if (sectionAxialOnly.count(e.section)) {
        out.report.rejected.push_back("element " + it.key() + ": BEAM with an angle (L) section is not supported -- only TRUSS members may use it (MIDAS's beam orientation of an angle is unverified)");
        continue;
      }
      beamNodes.insert(e.node1); beamNodes.insert(e.node2);
      try {
        Eigen::Vector3d p1(n1->x, n1->y, n1->z), p2(n2->x, n2->y, n2->z);
        const Real angleDeg = v.value("ANGLE", Real(0));
        const Eigen::Vector3d y0 = midasReferenceY((p2 - p1).normalized(), angleDeg);
        e.reference_y = y0;
        if (auto off = sectionOffsetDown.find(e.section); off != sectionOffsetDown.end()) {
          // "CT": member axis (centroid) at topFiber below the node along local -z (same at both ends).
          const Eigen::Vector3d z = (p2 - p1).normalized().cross(y0).normalized();
          e.offset1 = e.offset2 = -off->second * z;
        }
      } catch (const std::exception& ex) {
        out.report.rejected.push_back("element " + it.key() + ": " + ex.what());
        continue;
      }
      // FRLS (beam end releases, "mafsal"/hinge): keyed by element id, ITEMS[0] has FLAG_I/
      // FLAG_J (7-character strings, same per-DOF-flag convention as CONS's CONSTRAINT -- first
      // 6 chars = UX,UY,UZ,RX,RY,RZ, confirmed by cross-checking against a real export's own
      // Reaction/Beam Force/Displacements results, verification/models/18; the 7th char's meaning
      // is unconfirmed, same as CONS, and is ignored, not guessed) and `bVALUE` (true = a partial/
      // semi-rigid release with its own stiffness via VALUE_I/VALUE_J, which Beam3DConnection's
      // boolean-only releases cannot express -- rejected, not approximated as a full release).
      if (dbg.contains("FRLS") && dbg.at("FRLS").contains(it.key())) {
        const auto& frls = dbg.at("FRLS").at(it.key());
        if (!frls.contains("ITEMS") || frls.at("ITEMS").empty()) { out.report.rejected.push_back("element " + it.key() + ": FRLS present with no ITEMS entry"); continue; }
        const auto& item = frls.at("ITEMS")[0];
        if (item.value("bVALUE", false)) { out.report.rejected.push_back("element " + it.key() + ": FRLS bVALUE=true (partial/semi-rigid release) is not implemented"); continue; }
        const std::string flagI = item.value("FLAG_I", std::string());
        const std::string flagJ = item.value("FLAG_J", std::string());
        if (flagI.size() < 6 || flagJ.size() < 6) { out.report.rejected.push_back("element " + it.key() + ": FRLS FLAG_I/FLAG_J too short"); continue; }
        for (int i = 0; i < 6; ++i) { e.releases[(std::size_t)i] = flagI[(std::size_t)i] != '0'; e.releases[(std::size_t)(i+6)] = flagJ[(std::size_t)i] != '0'; }
        if (flagI.size() > 6 || flagJ.size() > 6)
          out.report.assumptions.push_back("element " + it.key() + ": FRLS FLAG_I/FLAG_J has more than 6 characters; extra character(s) ignored (meaning unconfirmed, same as CONS, see docs/MIDAS_JSON_DATA_AUDIT.md 4.4)");
      }
      out.input.model.beams.push_back(e);
      out.report.originalToInternalElementId[id] = id;
    }
  out.report.beams = out.input.model.beams.size();

  for (Id node : trussNodes) {
    if (beamNodes.count(node)) continue; // also touched by a BEAM, which supplies rotational stiffness
    auto& flags = out.input.restraints[node]; // default-constructed (all false) if not already present
    bool added = false;
    for (int i = 3; i < 6; ++i) { if (!flags[(std::size_t)i]) added = true; flags[(std::size_t)i] = true; }
    if (added)
      out.report.assumptions.push_back("node " + std::to_string(node) + ": connected only by TRUSS elements, which carry no rotational stiffness (Truss3D.hpp); RX/RY/RZ auto-restrained, matching MIDAS's own implicit pin-jointed-truss convention (not present as an explicit CONS restraint in the export)");
  }

  // (Removed 2026-11-20) A former planar-model heuristic auto-restrained the 3 out-of-plane DOFs of
  // every node whenever the model's bounding box had zero extent along one global axis. That was
  // wrong for MIDAS 3D models (STYP = 0) that merely lie in a plane: e.g. model 02 (a cantilever on
  // the X axis loaded in Y) came out with all displacements zero, and model 09 (an L-shaped
  // cantilever in the X-Y plane loaded in Z) likewise -- MIDAS solved both as full 3D. The model
  // that motivated the heuristic (model 20, re-solved by the user as a MIDAS 2D model) exports
  // STYP = 1, which is now handled explicitly below; the plane restraint comes from STYP only.

  // STYP (MIDAS "Structure Type"), 2026-11-20. STYP = 1 is MIDAS's X-Z plane (2D) structure type:
  // UY, RX and RZ are suppressed at every node, whatever the geometry. Evidence: (a) model 20
  // (Y = 0 planar truss), which the user re-solved explicitly as a MIDAS "2D" model, exports
  // STYP = 1, while every 3D model exports STYP = 0; (b) model 22 exports STYP = 1 although its
  // geometry spans Y (10 m), and MIDAS's own results for it have DY = RX = RZ = 0 exactly at every
  // node in every load case, with constraint reactions (MX, MZ) at nodes that carry no CONS
  // record. Other nonzero STYP values (other planes / "constrain RZ") have not been verified and
  // are reported, not guessed.
  if (dbg.contains("STYP") && !dbg.at("STYP").empty()) {
    const auto& st = dbg.at("STYP").begin().value();
    const int styp = st.value("STYP", 0);
    if (styp == 1) {
      for (const auto& n : out.input.model.nodes) {
        auto& flags = out.input.restraints[n.id];
        flags[1] = flags[3] = flags[5] = true;
      }
      out.report.assumptions.push_back("STYP=1 (MIDAS X-Z plane structure type): UY, RX and RZ restrained at every node (see MidasJsonImporter.hpp)");
    } else if (styp != 0) {
      out.report.rejected.push_back("STYP=" + std::to_string(styp) + ": MIDAS structure type other than 3D (0) or X-Z plane (1) is not mapped; the model is analysed as fully 3D");
    }
  }

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
        if (cmd != "BEAM" || dir != "GZ" || (ltype != "UNILOAD" && ltype != "CONLOAD")) {
          out.report.rejected.push_back("member load on element " + it.key() + ", case '" + lc + "': unsupported CMD/TYPE/DIRECTION combination ('" + cmd + "'/'" + ltype + "'/'" + dir + "')");
          continue;
        }
        const auto d = detail::toRealVector(item.at("D"));
        const auto p = detail::toRealVector(item.at("P"));
        if (ltype == "UNILOAD") {
          MemberLoad ml; ml.element = element; ml.axes = LoadAxes::Global; ml.direction = 2;
          ml.w1 = p.at(0); ml.w2 = p.at(1); ml.start = d.at(0); ml.end = d.at(1);
          caseFor(lc).member.push_back(ml);
        } else { // CONLOAD: a single concentrated force at fractional position D[0], magnitude P[0]
          MemberPointLoad pl; pl.element = element; pl.axes = LoadAxes::Global; pl.direction = 2;
          pl.kind = MemberLoadKind::Force; pl.value = p.at(0); pl.position = d.at(0);
          caseFor(lc).point.push_back(pl);
        }
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
  // --- GTMP (beam temperature gradient), 2026-11-20. MIDAS manual "Temperature Gradient" (saved
  // copy supplied by the user): TZ = T2z - T1z, the temperature difference between the two
  // outermost faces in the element local z direction, T2z on the +z face (dialog diagram); TY =
  // T2y - T1y likewise, T2y on the +y face; equivalent moment M = alpha*E*I*dT/h, h = HZ/HY, or the
  // section's overall dimension when USE_HZ/USE_HY ("Use Section Hz/Hy") is set.
  // KOPRUQ's MemberThermalLoad gradient_*_delta_t is the opposite difference (ThermalLoads.hpp:
  // a positive gradient_z_delta_t bends a cantilever's tip toward +z, i.e. the -z face is the hot
  // one), so  gradient_z_delta_t = -TZ,  gradient_y_delta_t = -TY.  Proven from MIDAS's own results
  // of verification model 22 (TG case): back-calculating each box-girder element's EI with the
  // gradient fixed-end moments of either sign, only this sign reproduces the section's EIy and EIz
  // (Iy 13.67209 = TU-case value, Iz 68.70453 = MIDAS's own Izz; the other sign gives
  // element-to-element scatter), and with h = the section's full depth (3.15) and full width (10.0).
  // TYPE 1 = beam is the only type mapped (plate gradients do not apply to beam elements).
  if (dbg.contains("GTMP")) {
    std::map<Id, Id> elementSection;
    for (const auto& b : out.input.model.beams) elementSection[b.id] = b.section;
    for (auto it = dbg.at("GTMP").begin(); it != dbg.at("GTMP").end(); ++it) {
      const Id element = detail::parseId(it.key());
      for (const auto& item : it.value().at("ITEMS")) {
        const std::string lc = item.at("LCNAME").get<std::string>();
        const std::string where = "GTMP element " + it.key() + " (load case '" + lc + "')";
        if (item.value("TYPE", 1) != 1) { out.report.rejected.push_back(where + ": TYPE " + std::to_string(item.value("TYPE", 0)) + " is not a beam gradient (only TYPE 1 is mapped)"); continue; }
        const Real TZ = item.value("TZ", Real(0)), TY = item.value("TY", Real(0));
        Real hz = item.value("HZ", Real(0)), hy = item.value("HY", Real(0));
        const auto sec = elementSection.find(element);
        const auto ext = sec == elementSection.end() ? sectionExtent.end() : sectionExtent.find(sec->second);
        const bool needZ = TZ != 0 && item.value("USE_HZ", false), needY = TY != 0 && item.value("USE_HY", false);
        if ((needZ || needY) && ext == sectionExtent.end()) {
          out.report.rejected.push_back(where + ": 'Use Section Hz/Hy' requested but the element's section has no defined overall dimensions (missing/rejected section or a principal-axis-rotated shape)");
          continue;
        }
        if (needZ) hz = ext->second.first;
        if (needY) hy = ext->second.second;
        if ((TZ != 0 && !(hz > 0)) || (TY != 0 && !(hy > 0))) { out.report.rejected.push_back(where + ": gradient depth HZ/HY must be positive"); continue; }
        caseFor(lc).thermal.push_back(MemberThermalLoad{element, 0, -TY, TY != 0 ? hy : Real(0), -TZ, TZ != 0 ? hz : Real(0)});
        ++out.report.thermal_loads;
      }
    }
  }
  // BTMP (beam-section temperatures, a piecewise temperature profile over the section) is still
  // recognized but NOT mapped -- its conventions have not been verified against MIDAS results.
  if (dbg.contains("BTMP") && !dbg.at("BTMP").empty())
    out.report.rejected.push_back("BTMP: beam-section temperature records are present but not mapped (conventions not verified); only ETMP and GTMP temperatures are imported");

  out.report.load_cases = out.input.load_cases.size();

  // --- LCOM-GEN: load combinations (Phase 7G, 2026-11-19) ---
  // Each entry's vCOMB lists terms as either ANAL="ST" (a primitive load case, by its STLD name)
  // or ANAL="CB" (another LCOM-GEN combination, by its own name) with a FACTOR. Since static
  // analysis is linear, a combination-of-combinations is mathematically EXACT to expand into an
  // equivalent flat linear combination of primitive load cases alone (substitute each CB term's
  // own expansion, scaled by its factor, repeated recursively) -- this is done here at import
  // time, not a new solver capability; LinearStaticSolver's own LoadCombination::terms always
  // indexes primitive load cases, unchanged (confirmed against a real nested-combination sample,
  // verification/models/21, which references a combination from within another combination).
  // Only ACTIVE=="ACTIVE" and iTYPE==0 (the only values seen in any real sample) are accepted;
  // any other iTYPE (e.g. an envelope/max-min combination, which cannot be expressed as a single
  // linear superposition at all) is rejected explicitly, not silently approximated as linear.
  if (dbg.contains("LCOM-GEN")) {
    std::map<std::string, std::vector<std::tuple<std::string, std::string, Real>>> rawCombos; // name -> (ANAL, ref, factor) terms
    std::map<std::string, std::string> comboKey; // name -> original JSON key, for diagnostics only
    for (auto it = dbg.at("LCOM-GEN").begin(); it != dbg.at("LCOM-GEN").end(); ++it) {
      const auto& v = it.value();
      const std::string name = v.value("NAME", std::string());
      comboKey[name] = it.key();
      const std::string active = v.value("ACTIVE", std::string());
      const int iType = v.value("iTYPE", 0);
      if (active != "ACTIVE") { out.report.rejected.push_back("LCOM-GEN " + it.key() + " ('" + name + "'): ACTIVE='" + active + "' (only ACTIVE combinations are imported)"); continue; }
      if (iType != 0) { out.report.rejected.push_back("LCOM-GEN " + it.key() + " ('" + name + "'): iTYPE=" + std::to_string(iType) + " is not supported (only the simple linear-superposition type, iTYPE=0, is implemented)"); continue; }
      std::vector<std::tuple<std::string, std::string, Real>> terms;
      bool ok = true;
      if (v.contains("vCOMB"))
        for (const auto& term : v.at("vCOMB")) {
          const std::string anal = term.value("ANAL", std::string());
          const std::string ref = term.value("LCNAME", std::string());
          const Real factor = term.value("FACTOR", Real(1));
          if (anal != "ST" && anal != "CB") { out.report.rejected.push_back("LCOM-GEN " + it.key() + " ('" + name + "'): vCOMB term has unsupported ANAL='" + anal + "' (only ST/CB are implemented)"); ok = false; break; }
          terms.emplace_back(anal, ref, factor);
        }
      if (!ok) continue;
      rawCombos[name] = std::move(terms);
    }

    // Recursively expands `name` into pure (load case name, net factor) terms, substituting any
    // CB reference by its own expansion; cycle-safe via an explicit recursion stack (a genuine
    // cycle is invalid input, not something to silently break out of arbitrarily).
    std::map<std::string, std::vector<std::pair<std::string, Real>>> expanded;
    std::function<bool(const std::string&, std::vector<std::string>&, std::vector<std::pair<std::string, Real>>&)> expand =
      [&](const std::string& name, std::vector<std::string>& stack, std::vector<std::pair<std::string, Real>>& result) -> bool {
        if (auto cached = expanded.find(name); cached != expanded.end()) { for (const auto& t : cached->second) result.push_back(t); return true; }
        if (std::find(stack.begin(), stack.end(), name) != stack.end()) return false; // cycle
        auto found = rawCombos.find(name);
        if (found == rawCombos.end()) return false; // references a combo that was rejected above or never defined
        stack.push_back(name);
        std::vector<std::pair<std::string, Real>> acc;
        for (const auto& [anal, ref, factor] : found->second) {
          if (anal == "ST") {
            acc.emplace_back(ref, factor);
          } else {
            std::vector<std::pair<std::string, Real>> sub;
            if (!expand(ref, stack, sub)) { stack.pop_back(); return false; }
            for (const auto& [n, f] : sub) acc.emplace_back(n, f * factor);
          }
        }
        stack.pop_back();
        expanded[name] = acc;
        for (const auto& t : acc) result.push_back(t);
        return true;
      };

    for (const auto& [name, terms] : rawCombos) {
      (void)terms;
      std::vector<std::string> stack;
      std::vector<std::pair<std::string, Real>> flat;
      if (!expand(name, stack, flat)) {
        out.report.rejected.push_back("LCOM-GEN " + comboKey[name] + " ('" + name + "'): could not be expanded (a referenced load case/combination is missing, unsupported, or forms a cycle)");
        continue;
      }
      // Merge duplicate load-case references (a case reachable more than once across nested
      // combination branches) into a single summed factor, exactly as linear superposition requires.
      std::map<std::string, Real> merged;
      for (const auto& [n, f] : flat) merged[n] += f;
      LoadCombination lcomb; lcomb.name = name;
      bool allFound = true;
      for (const auto& [caseName, factor] : merged) {
        auto caseIt = caseIndex.find(caseName);
        if (caseIt == caseIndex.end()) { allFound = false; break; }
        lcomb.terms.emplace_back(caseIt->second, factor);
      }
      if (!allFound) { out.report.rejected.push_back("LCOM-GEN " + comboKey[name] + " ('" + name + "'): references a load case not present in this model"); continue; }
      out.input.combinations.push_back(lcomb);
    }
  }
  out.report.combinations = out.input.combinations.size();

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

      // Direction: SPLC's DIR names the horizontal plane the spectrum can be applied in (e.g.
      // "XY"), and ANGLE picks which axis of that plane, 0 degrees = DIR's first letter, 90
      // degrees = DIR's second letter (confirmed against two real MIDAS exports,
      // verification/models/16 and /17: model 16's single SPECX case is DIR="XY" ANGLE=0 ->
      // X, already verified; model 17 adds SPECY, DIR="XY" ANGLE=90 -> Y, same DIR string, only
      // ANGLE differs, so DIR alone cannot be the whole story). A response-spectrum analysis
      // envelopes modal response magnitudes, so 180/270 degrees give the same case as 0/90 (sign
      // does not matter) -- angle is reduced modulo 180 before matching. Any angle that is not a
      // multiple of 90 degrees within the named plane is a genuinely arbitrary direction
      // ResponseSpectrumInput::direction (global X/Y/Z only) cannot express, and is rejected, not
      // approximated.
      auto axisOf = [](char c) -> int { c = static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return c == 'X' ? 0 : c == 'Y' ? 1 : c == 'Z' ? 2 : -1; };
      int direction = -1;
      const double angle = splc.value("ANGLE", 0.0);
      const std::string dir = splc.value("DIR", std::string());
      if (dir.size() >= 2 && axisOf(dir[0]) >= 0 && axisOf(dir[1]) >= 0) {
        double angleMod = std::fmod(angle, 180.0); if (angleMod < 0) angleMod += 180.0;
        if (std::abs(angleMod) < 1e-6) direction = axisOf(dir[0]);
        else if (std::abs(angleMod - 90.0) < 1e-6) direction = axisOf(dir[1]);
        else { ok = false; reason = "ANGLE " + std::to_string(angle) + " is not a multiple of 90 degrees within DIR plane '" + dir + "' -- an arbitrary-angle spectrum direction is not supported"; }
      } else if (dir.size() == 1 && axisOf(dir[0]) >= 0) {
        if (angle != 0.0) { ok = false; reason = "ANGLE != 0 with single-axis DIR '" + dir + "' is not supported"; }
        else direction = axisOf(dir[0]);
      } else { ok = false; reason = "DIR '" + dir + "' not recognized (expected one or two of X/Y/Z)"; }
      if (ok && dir.size() > 1)
        out.report.assumptions.push_back("SPLC " + caseName + ": DIR='" + dir + "' ANGLE=" + std::to_string(angle) +
            " -- direction taken as axis '" + std::string(1, "XYZ"[direction]) + "' (0 deg = DIR's first letter, 90 deg = second, mod 180)");

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
