#pragma once
// Eurocode (EN 1992-1-1) concrete material library, addressed by grade
// name (e.g. "C35/45" or "EN-C35/45"). General-purpose, solver-core-level
// capability (Phase 7M): usable by native JSON models and by the MIDAS
// importer alike, not tied to either. Supersedes
// kopruq/io/midas/MidasMaterialLibrary.hpp's ad hoc EN(RC) formula,
// which that file should be migrated to call once this is in place
// (not done in this commit, to keep this change isolated and testable).
//
// Standard: EN 1992-1-1:2004+A1:2014, Table 3.1 ("Strength and
// deformation characteristics for concrete"). The grade designation
// "C<fck>/<fck,cube>" is the characteristic cylinder/cube compressive
// strength in MPa, taken directly from the name (not computed); every
// other property is derived from fck via the Table 3.1 formulas:
//   fcm   = fck + 8                                   [MPa]
//   fctm  = 0.30 * fck^(2/3)                           for fck <= 50 MPa
//   fctm  = 2.12 * ln(1 + fcm/10)                      for fck >  50 MPa
//   Ecm   = 22000 * (fcm/10)^0.3                       [MPa]
// Cross-checked against EN 1992-1-1's own published Table 3.1 values for
// several classes in tests/unit/eurocode_material_library_tests.cpp
// (within a tolerance that accounts for the table's own rounding of the
// unrounded formula result).
//
// Not given by EN 1992-1-1 Table 3.1 itself, taken from elsewhere in the
// Eurocode suite and disclosed as such (not silent defaults):
//   Poisson's ratio        = 0.2        (EN 1992-1-1 S3.1.3(4), uncracked concrete)
//   Thermal expansion coef. = 1.0e-5 /K  (EN 1992-1-1 S3.1.3(5), normal-weight concrete)
//   Unit weight             = 25 kN/m^3  (EN 1991-1-1 Annex A, normal-weight reinforced concrete)
// National Annex overrides of any of these are out of scope for this
// first implementation (recorded as "Recommended (no NA override)").
#include "../core/Material.hpp"
#include "../core/Types.hpp"
#include <cmath>
#include <stdexcept>
#include <string>

namespace kopruq::materials {

struct EurocodeConcreteProperties {
  std::string grade;                                  // as given, e.g. "C35/45"
  std::string standard{"EN 1992-1-1:2004+A1:2014"};
  std::string national_annex{"Recommended (no NA override)"};
  Real fck{}, fck_cube{}, fcm{}, fctm{}, Ecm{};         // MPa
  Real poisson_ratio{}, thermal_expansion_per_K{};      // dimensionless, 1/K
  Real unit_weight{};                                   // kN/m^3
  Real G() const { return Ecm / (2 * (1 + poisson_ratio)); } // MPa, derived
};

namespace detail {
// Strips an optional "EN-" prefix and parses "C<fck>/<fck,cube>".
inline void parseGrade(const std::string& grade, Real& fck, Real& fckCube) {
  std::string s = grade;
  if (s.rfind("EN-", 0) == 0) s = s.substr(3);
  if (s.empty() || s[0] != 'C') throw std::invalid_argument("Eurocode concrete grade must look like 'C<fck>/<fck,cube>', got '" + grade + "'");
  const auto slash = s.find('/');
  if (slash == std::string::npos) throw std::invalid_argument("Eurocode concrete grade must look like 'C<fck>/<fck,cube>', got '" + grade + "'");
  try {
    fck = std::stod(s.substr(1, slash - 1));
    fckCube = std::stod(s.substr(slash + 1));
  } catch (const std::exception&) {
    throw std::invalid_argument("could not parse numeric strength values from Eurocode concrete grade '" + grade + "'");
  }
}
} // namespace detail

inline EurocodeConcreteProperties getConcrete(const std::string& grade) {
  Real fck{}, fckCube{};
  detail::parseGrade(grade, fck, fckCube);
  if (!(fck > 0) || !(fckCube > 0)) throw std::invalid_argument("Eurocode concrete grade strengths must be positive: '" + grade + "'");
  if (!(fckCube > fck)) throw std::invalid_argument("Eurocode concrete grade's cube strength must exceed its cylinder strength: '" + grade + "'");

  EurocodeConcreteProperties p;
  p.grade = grade;
  p.fck = fck; p.fck_cube = fckCube;
  p.fcm = fck + 8;
  p.fctm = (fck <= 50) ? Real(0.30) * std::pow(fck, Real(2)/3) : Real(2.12) * std::log(1 + p.fcm/10);
  p.Ecm = 22000 * std::pow(p.fcm/10, Real(0.3));
  p.poisson_ratio = 0.2;
  p.thermal_expansion_per_K = 1.0e-5;
  p.unit_weight = 25.0;
  return p;
}

// Converts to this project's canonical-unit Material (kN/m^2 for elastic
// modulus, matching Model.hpp/Section.hpp -- EurocodeConcreteProperties
// itself deliberately stays in Eurocode's own units (MPa) so it reads the
// same as the published table; unit conversion happens only here, once,
// at the one place a caller actually needs our solver's units.
inline kopruq::Material toMaterial(kopruq::Id id, const EurocodeConcreteProperties& p) {
  return kopruq::Material{id, p.Ecm * 1000.0, p.poisson_ratio, p.unit_weight};
}

} // namespace kopruq::materials
