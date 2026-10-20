#include "ResultMatcher.h"
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

struct ComponentSpec { const char* excelName; int index; QuantityCategory category; };

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
                                           const std::array<ComponentSpec, 6>& components) {
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
    const bool caseKnown = findLoadCase(results, loadCase) != nullptr;
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
        const double solverValue = nodeIt->second->at(jsonField).at(static_cast<std::size_t>(components[k].index)).get<double>();
        out_row.result = compareValues(solverValue, cell.number, components[k].category, tol);
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
    const bool caseKnown = findLoadCase(results, loadCase) != nullptr;
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
        const double solverValue =
            elemIt->second->at("local_end_forces").at(static_cast<std::size_t>(endOffset + kComponents[k].index)).get<double>();
        out_row.result = compareValues(solverValue, cell.number, kComponents[k].category, tol);
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

} // namespace kopruq::verification
