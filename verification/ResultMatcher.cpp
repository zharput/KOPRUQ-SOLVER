#include "ResultMatcher.h"
#include <algorithm>
#include <cmath>
#include <array>
#include <map>

namespace kopruq::verification {

namespace {

using Json = nlohmann::json;

// Finds a load case entry by MIDAS's own load case name (preserved unchanged by Phase 7B's
// importer from STLD's NAME field). Returns nullptr if not present in results.json at all.
const Json* findLoadCase(const Json& results, const std::string& name) {
  if (!results.contains("load_cases")) return nullptr;
  for (const auto& lc : results.at("load_cases"))
    if (lc.value("name", std::string{}) == name) return &lc;
  if (results.contains("combinations"))
    for (const auto& lc : results.at("combinations"))
      if (lc.value("name", std::string{}) == name) return &lc;
  return nullptr;
}

long toId(double v) { return static_cast<long>(v + (v >= 0 ? 0.5 : -0.5)); }

// A response-spectrum case appended by appendSpectrumCases: its values are peak magnitudes.
bool isPeakMagnitudeCase(const Json* lc) { return lc && lc->value("peak_magnitude", false); }

struct ComponentSpec { const char* excelName; int index; QuantityCategory category; };

// Phase 7E.1 (2026-11-17) -- MIDAS's own "Beam Force" export and
// Beam3DConnection/LinearStaticSolver's `local_end_force` (the production value compared here)
// use two DIFFERENT, independently-derived sign conventions for a beam's end actions, confirmed
// by cross-checking already-independently-verified solver output (Phase R1-R4, Beam3D core)
// against real, solved MIDAS reference exports -- never by adjusting this rule until a
// comparison test passed.
//
// Derivation (axial, the simplest case -- model "01 Konsol_Fx", a pure-tension cantilever, FX=500
// pulling outward at the free tip, fixed at the other end): hand statics give a single,
// unambiguous internal axial force N = +500 kN (tension-positive) at every cross-section of every
// element. MIDAS's own export reports exactly that, +500, at BOTH the I and J end of every
// element -- i.e. MIDAS's Axial/Shear/Torsion columns are the direct, station-independent
// "diagram" value (same physical quantity, same sign, everywhere along the member, exactly as
// taught in statics: N(x), V(x), T(x)). `local_end_force` (LinearStaticSolver.hpp's own comment:
// "End forces exerted by the nodes on the element: k*u minus the member loads' equivalent nodal
// loads") is instead the force the JOINT exerts ON the element at that end -- for the member to
// be in tension (N>0), the joint at the start (I) must pull the element backward/outward (-local
// x), while the joint at the end (J) must pull it forward/outward (+local x): algebraically
// f_I = -N, f_J = +N. This identity holds independent of this specific model (it is how N(x) is
// *defined* via a free-body cut, not a property of any one load case), and was confirmed to hold
// exactly (within numerical tolerance, zero counter-examples) across every Axial/Shear-z/Moment-y
// mismatch in models "05 Cerceve-1" (147 of 147 rows) and "06 Cerceve-2" (162 of 162 rows) --
// including the loaded horizontal beam elements (BMLD), not just the unloaded columns, confirming
// the relationship is NOT an artifact of one particular element orientation or load pattern.
//
// Bending moments behave oppositely (f_I = +M, f_J = -M, i.e. the SAME value needs no flip at the
// I end but does at the J end) -- this is because a bending moment, unlike axial/shear/torsion,
// is coupled to the end shear through a moment-arm term (dM/dx = V) that reverses which end's
// nodal-force convention lines up with the diagram convention; empirically confirmed for
// Moment-y the same way (148 model-05 + 162 model-06 rows, zero counter-examples).
//
// Shear-y and Torsion are extended to the SAME group as their already-confirmed counterpart
// (Shear-z/Axial -- all are "arm-free" resultants with no dM/dx coupling, by the identical
// argument above) by this physical reasoning, NOT by direct empirical confirmation: models 01-06
// are planar (Y=0 throughout, in-plane loading only), so Shear-y/Torsion are structurally zero in
// all of them and no comparison row has exercised this part of the rule. Disclosed, not hidden,
// in case a future out-of-plane model exposes a counter-example.
//
// Moment-z is NOT the same group as Moment-y, despite both being "bending moments with arm
// coupling" -- empirically confirmed 2026-11-19 against model "18 Köprü_mafsal" (real MIDAS
// export with genuine out-of-plane bending, the first available model where Moment-z is
// structurally nonzero: vertical pylon members 62-71, section 4). The original symmetry argument
// assumed the xy bending plane (Moment-z) behaves like the xz bending plane (Moment-y), but the
// two planes are NOT symmetric under the right-handed local-axis convention: z = x cross y
// (direct), while y = z cross x (an extra cyclic step) -- this antisymmetry between the two
// bending planes is exactly why Moment-z's I/J flip is the OPPOSITE of Moment-y's, not the same.
// Confirmed by the data itself: with the old (false) assumption, every Moment-z row for elements
// 62-71 (both I and J ends) was an exact sign flip against MIDAS's real reference value; setting
// it to `true` (matching the Axial/Shear/Torsion group instead of Moment-y) resolves every one of
// them exactly, to within the material-value rounding already present elsewhere in this model.
constexpr std::array<bool, 6> kBeamForceFlipsAtIEnd = {
    true,  // Axial -- empirically confirmed (above)
    true,  // Shear-y -- extended by symmetry with Shear-z, not yet empirically exercised
    true,  // Shear-z -- empirically confirmed (above)
    true,  // Torsion -- extended by symmetry with Axial, not yet empirically exercised
    false, // Moment-y -- empirically confirmed to flip at J instead (above)
    true,  // Moment-z -- empirically confirmed (above, model 18) -- NOT the same group as Moment-y
};
// `midasValue`: the MIDAS reference cell's own value, already in MIDAS's own convention; no
// conversion needed on that side. What this returns is the SOLVER's `local_end_force` component,
// converted into MIDAS's convention for the comparison -- never the other way around (the
// reference data is ground truth here, never adjusted).
inline double toMidasBeamForceConvention(double solverLocalEndForce, bool isIEnd, std::size_t componentIndex) {
  const bool flipsAtIEnd = kBeamForceFlipsAtIEnd[componentIndex];
  const bool flip = isIEnd ? flipsAtIEnd : !flipsAtIEnd;
  return flip ? -solverLocalEndForce : solverLocalEndForce;
}

// Reads one MIDAS id-like cell (exported as a number, e.g. "1.0") as an integer; returns false
// (and leaves id untouched) if the cell isn't numeric -- callers diagnose rather than guess 0.
bool readIdCell(const CellValue& cell, long& id) {
  if (!cell.isNumber) return false;
  id = toId(cell.number);
  return true;
}

// Shared by Displacements (node.displacement, DX/DY/DZ/RX/RY/RZ) and Reactions
// (node.reaction, FX/FY/FZ/MX/MY/MZ) -- identical structure, different field/column names.
CategoryComparison matchNodeBasedCategory(const Json& results, const ReferenceTable& ref,
                                           const ToleranceSettings& tol, const char* jsonField,
                                           const std::array<ComponentSpec, 6>& components,
                                           const char* fallbackField = nullptr) {
  CategoryComparison out;
  const int nodeCol = ref.columnIndex("Node");
  const int loadCol = ref.columnIndex("Load");
  if (nodeCol < 0 || loadCol < 0) {
    out.diagnostics.push_back("Beklenen sütunlar bulunamadı (Node/Load).");
    return out;
  }
  std::vector<int> compCols;
  for (const auto& c : components) compCols.push_back(ref.columnIndex(c.excelName));

  std::map<std::string, std::map<long, const Json*>> nodeCache; // load case name -> node id -> entry
  for (const auto& row : ref.rows) {
    const std::string loadCase = row[loadCol].text;
    long nodeId = 0;
    if (!readIdCell(row[nodeCol], nodeId)) { out.diagnostics.push_back("Geçersiz Node değeri (satır atlandı)."); continue; }

    auto cacheIt = nodeCache.find(loadCase);
    if (cacheIt == nodeCache.end()) {
      std::map<long, const Json*> m;
      if (const Json* lc = findLoadCase(results, loadCase); lc && lc->contains("nodes"))
        for (const auto& n : lc->at("nodes")) m[toId(n.at("id").get<double>())] = &n;
      cacheIt = nodeCache.emplace(loadCase, std::move(m)).first;
    }
    const auto nodeIt = cacheIt->second.find(nodeId);
    const Json* lcEntry = findLoadCase(results, loadCase);
    const bool caseKnown = lcEntry != nullptr;
    const bool peak = isPeakMagnitudeCase(lcEntry);
    if (nodeIt == cacheIt->second.end() && !caseKnown)
      out.diagnostics.push_back("Yük durumu results.json'da bulunamadı: " + loadCase);

    for (std::size_t k = 0; k < components.size(); ++k) {
      if (compCols[k] < 0) continue; // this component's column isn't present in this export; nothing to compare
      const CellValue& cell = row[compCols[k]];
      if (!cell.isNumber) continue; // a blank/non-numeric reference cell is not a value to compare against
      ComparisonRow out_row;
      out_row.loadCase = loadCase;
      out_row.id = std::to_string(nodeId);
      out_row.component = components[k].excelName;
      out_row.unit = ref.columns[compCols[k]].unit;
      if (nodeIt == cacheIt->second.end()) {
        out_row.result.status = ComparisonStatus::NotApplicable;
        out_row.result.referenceValue = cell.number;
      } else {
        const Json& node = *nodeIt->second;
        const Json& values = (fallbackField && !node.contains(jsonField)) ? node.at(fallbackField) : node.at(jsonField);
        const double solverValue = values.at(static_cast<std::size_t>(components[k].index)).get<double>();
        out_row.result = peak ? compareValues(std::abs(solverValue), std::abs(cell.number), components[k].category, tol)
                              : compareValues(solverValue, cell.number, components[k].category, tol);
      }
      out.rows.push_back(std::move(out_row));
    }
  }
  return out;
}

} // namespace

CategoryComparison matchDisplacements(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  static const std::array<ComponentSpec, 6> kComponents{{
      {"DX", 0, QuantityCategory::Displacement}, {"DY", 1, QuantityCategory::Displacement}, {"DZ", 2, QuantityCategory::Displacement},
      {"RX", 3, QuantityCategory::Rotation}, {"RY", 4, QuantityCategory::Rotation}, {"RZ", 5, QuantityCategory::Rotation}}};
  return matchNodeBasedCategory(results, ref, tol, "displacement", kComponents);
}

CategoryComparison matchReactions(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  static const std::array<ComponentSpec, 6> kComponents{{
      {"FX", 0, QuantityCategory::Force}, {"FY", 1, QuantityCategory::Force}, {"FZ", 2, QuantityCategory::Force},
      {"MX", 3, QuantityCategory::Moment}, {"MY", 4, QuantityCategory::Moment}, {"MZ", 5, QuantityCategory::Moment}}};
  return matchNodeBasedCategory(results, ref, tol, "reaction", kComponents);
}

CategoryComparison matchLocalReactions(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  // MIDAS "Reaction(Local)" headers are Fx/Fy/Fz/Mx/My/Mz (columnIndex is case-insensitive). A node
  // without local axes has no "reaction_local" in results.json: its local reaction is the global one.
  static const std::array<ComponentSpec, 6> kComponents{{
      {"Fx", 0, QuantityCategory::Force}, {"Fy", 1, QuantityCategory::Force}, {"Fz", 2, QuantityCategory::Force},
      {"Mx", 3, QuantityCategory::Moment}, {"My", 4, QuantityCategory::Moment}, {"Mz", 5, QuantityCategory::Moment}}};
  return matchNodeBasedCategory(results, ref, tol, "reaction_local", kComponents, "reaction");
}

CategoryComparison matchElasticLinks(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  CategoryComparison out;
  static const std::array<ComponentSpec, 6> kComponents{{
      {"Axial", 0, QuantityCategory::Force}, {"Shear-y", 1, QuantityCategory::Force}, {"Shear-z", 2, QuantityCategory::Force},
      {"Torsion", 3, QuantityCategory::Moment}, {"Moment-y", 4, QuantityCategory::Moment}, {"Moment-z", 5, QuantityCategory::Moment}}};
  const int linkCol = ref.columnIndex("No.");
  const int loadCol = ref.columnIndex("Load");
  const int nodeCol = ref.columnIndex("Node");
  if (linkCol < 0 || loadCol < 0) {
    out.diagnostics.push_back("Beklenen sütunlar bulunamadı (No./Load).");
    return out;
  }
  std::vector<int> compCols;
  for (const auto& c : kComponents) compCols.push_back(ref.columnIndex(c.excelName));
  std::map<std::string, std::map<long, const Json*>> cache;
  bool missingSprings = false;
  for (const auto& row : ref.rows) {
    const std::string loadCase = row[loadCol].text;
    long linkId = 0;
    if (!readIdCell(row[linkCol], linkId)) { out.diagnostics.push_back("Geçersiz No. değeri (satır atlandı)."); continue; }
    auto it = cache.find(loadCase);
    if (it == cache.end()) {
      std::map<long, const Json*> m;
      const Json* lc = findLoadCase(results, loadCase);
      if (lc && lc->contains("springs"))
        for (const auto& e : lc->at("springs")) m[toId(e.at("id").get<double>())] = &e;
      else if (lc && !isPeakMagnitudeCase(lc)) missingSprings = true;
      if (!lc) out.diagnostics.push_back("Yük durumu results.json'da bulunamadı: " + loadCase);
      it = cache.emplace(loadCase, std::move(m)).first;
    }
    const bool peak = isPeakMagnitudeCase(findLoadCase(results, loadCase));
    const auto linkIt = it->second.find(linkId);
    for (std::size_t k = 0; k < kComponents.size(); ++k) {
      if (compCols[k] < 0) continue;
      const CellValue& cell = row[compCols[k]];
      if (!cell.isNumber) continue;
      ComparisonRow out_row;
      out_row.loadCase = loadCase;
      out_row.id = std::to_string(linkId);
      out_row.location = (nodeCol >= 0 && row[nodeCol].isNumber) ? std::to_string(toId(row[nodeCol].number)) : std::string{};
      out_row.component = kComponents[k].excelName;
      out_row.unit = ref.columns[compCols[k]].unit;
      if (linkIt == it->second.end()) {
        out_row.result.status = ComparisonStatus::NotApplicable;
        out_row.result.referenceValue = cell.number;
      } else {
        // MIDAS reports one internal link force per component (identical on both node rows). It is
        // the node2 (J) end force local_end_forces[6 + k] (SpringLink.hpp: k (u2 - u1), link-local
        // axes) in MIDAS's beam-force convention at the J end: every component as is except
        // Moment-y, which flips (kBeamForceFlipsAtIEnd above). Evidence, verification model 23:
        // links 5/6 (the only ones with rotational stiffness), cases UNIFORM/POINT/TRAPEZ -- Axial,
        // Shear-y/z, Torsion and Moment-z agree in sign and value, Moment-y has equal magnitude and
        // opposite sign in all 12 rows (e.g. MIDAS -2.66 vs node2 end force +2.663).
        const double v = toMidasBeamForceConvention(linkIt->second->at("local_end_forces").at(6 + k).get<double>(), /*isIEnd=*/false, k);
        out_row.result = peak ? compareValues(std::abs(v), std::abs(cell.number), kComponents[k].category, tol)
                              : compareValues(v, cell.number, kComponents[k].category, tol);
      }
      out.rows.push_back(std::move(out_row));
    }
  }
  if (missingSprings)
    out.diagnostics.push_back("results.json bazı yük durumlarında elastik bağlantı sonucu içermiyor (bağlantılar içe aktarılmamış olabilir): ilgili satırlar N/A.");
  return out;
}

CategoryComparison matchBeamForces(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  CategoryComparison out;
  static const std::array<ComponentSpec, 6> kComponents{{
      {"Axial", 0, QuantityCategory::Force}, {"Shear-y", 1, QuantityCategory::Force}, {"Shear-z", 2, QuantityCategory::Force},
      {"Torsion", 3, QuantityCategory::Moment}, {"Moment-y", 4, QuantityCategory::Moment}, {"Moment-z", 5, QuantityCategory::Moment}}};

  const int elemCol = ref.columnIndex("Elem");
  const int loadCol = ref.columnIndex("Load");
  const int partCol = ref.columnIndex("Part");
  if (elemCol < 0 || loadCol < 0 || partCol < 0) {
    out.diagnostics.push_back("Beklenen sütunlar bulunamadı (Elem/Load/Part).");
    return out;
  }
  std::vector<int> compCols;
  for (const auto& c : kComponents) compCols.push_back(ref.columnIndex(c.excelName));

  std::map<std::string, std::map<long, const Json*>> elemCache;
  for (const auto& row : ref.rows) {
    const std::string loadCase = row[loadCol].text;
    long elemId = 0;
    if (!readIdCell(row[elemCol], elemId)) continue; // a continuation/blank row (MIDAS repeats blanks under "Elem")

    const std::string part = row[partCol].text;
    if (part.empty() || (part[0] != 'I' && part[0] != 'J')) {
      out.diagnostics.push_back("Beklenmeyen 'Part' değeri (ne I ne J ile başlıyor): '" + part + "'");
      continue;
    }
    const int endOffset = (part[0] == 'I') ? 0 : 6;

    auto cacheIt = elemCache.find(loadCase);
    if (cacheIt == elemCache.end()) {
      std::map<long, const Json*> m;
      if (const Json* lc = findLoadCase(results, loadCase); lc && lc->contains("elements"))
        for (const auto& e : lc->at("elements")) m[toId(e.at("id").get<double>())] = &e;
      cacheIt = elemCache.emplace(loadCase, std::move(m)).first;
    }
    const auto elemIt = cacheIt->second.find(elemId);
    const Json* lcEntry = findLoadCase(results, loadCase);
    const bool caseKnown = lcEntry != nullptr;
    const bool peak = isPeakMagnitudeCase(lcEntry);
    if (elemIt == cacheIt->second.end() && !caseKnown)
      out.diagnostics.push_back("Yük durumu results.json'da bulunamadı: " + loadCase);

    for (std::size_t k = 0; k < kComponents.size(); ++k) {
      if (compCols[k] < 0) continue;
      const CellValue& cell = row[compCols[k]];
      if (!cell.isNumber) continue;
      ComparisonRow out_row;
      out_row.loadCase = loadCase;
      out_row.id = std::to_string(elemId);
      out_row.location = (part[0] == 'I') ? "I" : "J";
      out_row.component = kComponents[k].excelName;
      out_row.unit = ref.columns[compCols[k]].unit;
      if (elemIt == cacheIt->second.end()) {
        out_row.result.status = ComparisonStatus::NotApplicable;
        out_row.result.referenceValue = cell.number;
      } else {
        const double solverValueRaw =
            elemIt->second->at("local_end_forces").at(static_cast<std::size_t>(endOffset + kComponents[k].index)).get<double>();
        if (peak) {
          out_row.result = compareValues(std::abs(solverValueRaw), std::abs(cell.number), kComponents[k].category, tol);
        } else {
          const double solverValue = toMidasBeamForceConvention(solverValueRaw, part[0] == 'I', k);
          out_row.result = compareValues(solverValue, cell.number, kComponents[k].category, tol);
        }
      }
      out.rows.push_back(std::move(out_row));
    }
  }
  return out;
}

CategoryComparison matchTrussForces(const Json& results, const ReferenceTable& ref, const ToleranceSettings& tol) {
  CategoryComparison out;
  const int elemCol = ref.columnIndex("Elem");
  const int loadCol = ref.columnIndex("Load");
  const int forceICol = ref.columnIndex("Force-I");
  const int forceJCol = ref.columnIndex("Force-J");
  if (elemCol < 0 || loadCol < 0 || forceICol < 0 || forceJCol < 0) {
    out.diagnostics.push_back("Beklenen sütunlar bulunamadı (Elem/Load/Force-I/Force-J).");
    return out;
  }

  std::map<std::string, std::map<long, const Json*>> elemCache;
  for (const auto& row : ref.rows) {
    const std::string loadCase = row[loadCol].text;
    long elemId = 0;
    if (!readIdCell(row[elemCol], elemId)) continue;

    auto cacheIt = elemCache.find(loadCase);
    if (cacheIt == elemCache.end()) {
      std::map<long, const Json*> m;
      if (const Json* lc = findLoadCase(results, loadCase); lc && lc->contains("elements"))
        for (const auto& e : lc->at("elements")) m[toId(e.at("id").get<double>())] = &e;
      cacheIt = elemCache.emplace(loadCase, std::move(m)).first;
    }
    const auto elemIt = cacheIt->second.find(elemId);
    const bool caseKnown = findLoadCase(results, loadCase) != nullptr;
    if (elemIt == cacheIt->second.end() && !caseKnown)
      out.diagnostics.push_back("Yük durumu results.json'da bulunamadı: " + loadCase);

    const struct { int col; bool isIEnd; const char* location; } ends[2] = {
        {forceICol, true, "I"}, {forceJCol, false, "J"}};
    for (const auto& end : ends) {
      const CellValue& cell = row[end.col];
      if (!cell.isNumber) continue;
      ComparisonRow out_row;
      out_row.loadCase = loadCase;
      out_row.id = std::to_string(elemId);
      out_row.location = end.location;
      out_row.component = "Axial";
      out_row.unit = ref.columns[static_cast<std::size_t>(end.col)].unit;
      if (elemIt == cacheIt->second.end()) {
        out_row.result.status = ComparisonStatus::NotApplicable;
        out_row.result.referenceValue = cell.number;
      } else {
        const double solverValueRaw = elemIt->second->at("local_end_forces").at(end.isIEnd ? 0 : 6).get<double>();
        const double solverValue = toMidasBeamForceConvention(solverValueRaw, end.isIEnd, 0); // Axial = component 0
        out_row.result = compareValues(solverValue, cell.number, QuantityCategory::Force, tol);
      }
      out.rows.push_back(std::move(out_row));
    }
  }
  return out;
}

CategoryComparison markElasticLinksUnsupported(const ReferenceTable& ref) {
  CategoryComparison out;
  const int linkCol = ref.columnIndex("No.");
  const int loadCol = ref.columnIndex("Load");
  const int nodeCol = ref.columnIndex("Node");
  for (const auto& row : ref.rows) {
    ComparisonRow out_row;
    out_row.loadCase = (loadCol >= 0) ? row[loadCol].text : std::string{};
    out_row.id = (linkCol >= 0 && row[linkCol].isNumber) ? std::to_string(toId(row[linkCol].number)) : std::string{};
    out_row.location = (nodeCol >= 0 && row[nodeCol].isNumber) ? std::to_string(toId(row[nodeCol].number)) : std::string{};
    out_row.result.status = ComparisonStatus::Unsupported;
    out.rows.push_back(std::move(out_row));
  }
  if (!ref.rows.empty())
    out.diagnostics.push_back(
        "Elastik bağlantı sonuçları bu modelde desteklenmiyor: MIDAS ELNK kayıtları (eş merkezli "
        "olmayan düğümler) Phase 7B içe aktarıcısı tarafından kabul edilmiyor (gerçek bir "
        "formülasyon sınırı, eksik veri değil).");
  return out;
}

CategoryComparison matchEigenvalueFrequencies(const Json& modalResults, const std::vector<EigenvalueReferenceRow>& eigenRef,
                                               const ToleranceSettings& tol) {
  CategoryComparison out;
  const bool solved = modalResults.contains("status") && modalResults.at("status").value("success", false);
  if (!solved) out.diagnostics.push_back("Modal analiz sonucu başarısız veya mevcut değil: tüm mod karşılaştırmaları N/A.");

  std::map<int, const Json*> byIndex; // 0-based ModalSolver mode index -> its JSON entry
  if (solved && modalResults.contains("modes"))
    for (const auto& m : modalResults.at("modes")) byIndex[m.at("index").get<int>()] = &m;

  for (const auto& ref : eigenRef) {
    const int zeroBased = ref.mode - 1; // reference Mode No is 1-based, ModalSolver's own index is 0-based
    const auto it = byIndex.find(zeroBased);
    const struct { const char* label; double EigenvalueReferenceRow::*field; QuantityCategory category; const char* jsonField; } comps[] = {
        {"Frekans (Hz)", &EigenvalueReferenceRow::frequency_hz, QuantityCategory::Frequency, "frequency_hz"},
        {"Açısal Frekans (rad/s)", &EigenvalueReferenceRow::angular_frequency_rad_s, QuantityCategory::Frequency, "angular_frequency_rad_s"},
    };
    for (const auto& c : comps) {
      ComparisonRow row;
      row.id = std::to_string(ref.mode);
      row.component = c.label;
      row.unit = (c.jsonField == std::string("frequency_hz")) ? "Hz" : "rad/s";
      const double referenceValue = ref.*(c.field);
      if (it == byIndex.end()) {
        row.result.status = ComparisonStatus::NotApplicable;
        row.result.referenceValue = referenceValue;
      } else {
        const double solverValue = it->second->at(c.jsonField).get<double>();
        row.result = compareValues(solverValue, referenceValue, c.category, tol);
      }
      out.rows.push_back(std::move(row));
    }
  }
  return out;
}

void appendSpectrumCases(Json& results, const Json& spectrumResults) {
  if (!spectrumResults.contains("cases")) return;
  if (!results.contains("load_cases") || !results.at("load_cases").is_array()) results["load_cases"] = Json::array();
  for (const auto& c : spectrumResults.at("cases")) {
    if (!c.contains("status") || !c.at("status").value("success", false)) continue;
    const std::string name = c.value("name", std::string{}) + "(RS)";
    if (findLoadCase(results, name)) continue;
    Json lc = {{"name", name}, {"status", c.at("status")}, {"peak_magnitude", true}, {"nodes", Json::array()}, {"elements", Json::array()}};
    if (c.contains("combined_nodes"))
      for (const auto& n : c.at("combined_nodes"))
        lc["nodes"].push_back({{"id", n.at("id")}, {"displacement", n.at("displacement")}, {"reaction", n.at("reaction")}});
    if (c.contains("combined_elements"))
      for (const auto& e : c.at("combined_elements"))
        lc["elements"].push_back({{"id", e.at("id")}, {"local_end_forces", e.at("local_end_forces")}});
    results["load_cases"].push_back(std::move(lc));
  }
}

CategoryStats summarizeCategory(const std::vector<ComparisonRow>& rows) {
  CategoryStats s;
  s.total = static_cast<int>(rows.size());
  for (const auto& r : rows) {
    switch (r.result.status) {
      case ComparisonStatus::Pass: ++s.pass; break;
      case ComparisonStatus::Fail: ++s.fail; break;
      default: ++s.na; break;
    }
    if (r.result.status == ComparisonStatus::Pass || r.result.status == ComparisonStatus::Fail) {
      s.maxAbsDiff = std::max(s.maxAbsDiff, r.result.absDiff);
      s.maxAbsDiffPercent = std::max(s.maxAbsDiffPercent, r.result.relDiff * 100.0);
    }
  }
  return s;
}

} // namespace kopruq::verification
