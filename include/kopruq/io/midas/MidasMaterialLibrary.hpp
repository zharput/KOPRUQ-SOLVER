#pragma once
// Resolves a MIDAS Civil NX MATL/MATD "design-code grade" material (e.g.
// "C35/45" under code family "EN04(RC)") into KOPRUQ-SOLVER's Material
// (elastic_modulus, poisson_ratio, unit_weight). The MIDAS JSON export
// gives a grade name and characteristic strength, never E/Poisson/density
// directly -- see docs/MIDAS_JSON_DATA_AUDIT.md section 4.2.
//
// Elastic modulus: for the standard-named EN1992-1-1 grades listed in
// kTabulatedEcmGPa below, the FIXED, rounded Ecm value published in
// EN1992-1-1 Table 3.1 is used (e.g. C35/45 -> 34 GPa). EN1992-1-1's own
// footnote under Table 3.1 states the table's values are "for information"
// and the formula Ecm=22*((fcm)/10)^0.3 GPa (fcm=fck+8 MPa; equivalently
// 9.5*(fck+8)^(1/3)*1e3 MPa -- the two are the same curve, just expressed
// with different exponents/constants) only approximates them -- the table
// is the one actually bound to a NAMED grade in design software, not the
// continuous formula. Empirical cross-check against the real MIDAS
// reference results in verification/models/ (Phase 7B requalification,
// 2026-11-16): Model 04 (10 m simply-supported beam, C35/45, uniform
// load)'s MIDAS midspan deflection is smaller than this importer's
// Euler-Bernoulli closed-form result by 2.13% -- compare to
// 1 - 33.2823/34.0 = 2.11%, i.e. almost exactly the gap between the
// formula-derived and table Ecm for this grade. This is strong
// quantitative evidence, not a confirmed read of MIDAS's own source (its
// support site returns HTTP 403 to automated fetches, and the Analysis
// Reference PDF linked from MIDAS's own manuals KB is blocked by Dropbox
// for the same reason) -- so it is applied as a disclosed, named-grade
// table lookup, not silently assumed, and grades NOT in the table still
// fall back to the formula (also disclosed, see the "formula fallback"
// assumption below). Per explicit user approval, 2026-11-16, after
// reviewing this evidence (CLAUDE.md: changes to material formulation
// require approval).
//
// Only code names containing "EN" and "(RC)" are accepted; anything else
// is rejected with a clear diagnostic rather than guessed at.
//
// Poisson's ratio, unit weight and thermal expansion are not present
// anywhere in the export for a grade-named material. Per explicit user
// decision (2026-10-18, Phase 7B kickoff), standard code-default values
// are substituted rather than failing the import, and the substitution
// is always recorded (never silent) in the returned
// ResolvedMaterial::assumptions, which the importer's reconstruction
// report must surface to the caller:
//   Poisson's ratio    = 0.2        (EN1992-1-1 S3.1.3(4), concrete, linear analysis)
//   Unit weight         = 25 kN/m^3  (EN1991-1-1, normal-weight reinforced concrete)
//   Thermal expansion   = 1.0e-5 /K  (EN1992-1-1 S3.1.3(5), normal-weight concrete --
//                                     needed so ETMP/thermal loads, Phase 7T, actually
//                                     produce a nonzero effect rather than silently
//                                     computing to zero through a zero alpha_T)
#include "../../core/Material.hpp"
#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace kopruq::io::midas {

// One disclosed substitution: which field, what value was used, and why -- so a
// reconstruction report can print "assumed poisson_ratio=0.2 (EN1992-1-1 S3.1.3)"
// instead of silently using the number.
struct MaterialAssumption { std::string field, value, source; };

struct ResolvedMaterial { kopruq::Material material; std::vector<MaterialAssumption> assumptions; };

namespace detail {
// EN1992-1-1 Table 3.1, "Strength and deformation characteristics for concrete" --
// the fixed, rounded Ecm [GPa] published against each named normal-strength grade
// (fck <= 50 MPa; high-strength classes C55/67 and above use Annex formulas not
// reproduced here, so they intentionally fall back to the general formula below).
inline const std::map<std::string, double>& tabulatedEcmGPa() {
  static const std::map<std::string, double> table = {
      {"C12/15", 27.0}, {"C16/20", 29.0}, {"C20/25", 30.0}, {"C25/30", 31.0},
      {"C30/37", 33.0}, {"C35/45", 34.0}, {"C40/50", 35.0}, {"C45/55", 36.0},
      {"C50/60", 37.0},
  };
  return table;
}
} // namespace detail

// codeName: MATD's DATA1.CODENAME (e.g. "EN04(RC)"). fckMPa: MATD's DATA1.DESIGN.C_FC,
// already in MPa (the export's C_FC is in kN/m^2 = 1000x the MPa value; caller divides).
// gradeName: MATD's DATA1.CODEMATLNAME (e.g. "C35/45"), used only to look up the
// Table 3.1 fixed Ecm value; the formula fallback below needs only fckMPa.
inline ResolvedMaterial resolveConcreteGradeMaterial(kopruq::Id id, const std::string& codeName, double fckMPa, const std::string& gradeName = {}) {
  if (codeName.find("EN") == std::string::npos || codeName.find("(RC)") == std::string::npos)
    throw std::invalid_argument("unsupported material design code '" + codeName + "': only the EN..(RC) family's E-fck formula is implemented (see MidasMaterialLibrary.hpp)");
  if (!std::isfinite(fckMPa) || fckMPa <= 0)
    throw std::invalid_argument("material characteristic strength must be finite and positive");

  double EcmMPa; std::string ecmSource;
  const auto& table = detail::tabulatedEcmGPa();
  if (const auto found = table.find(gradeName); found != table.end()) {
    EcmMPa = found->second * 1000.0;
    ecmSource = "EN1992-1-1 Table 3.1 fixed/rounded Ecm value for named grade '" + gradeName +
                "' (not the continuous fck-based formula; cross-checked against MIDAS reference results, "
                "see MidasMaterialLibrary.hpp header comment, 2026-11-16)";
  } else {
    EcmMPa = 9.5 * std::cbrt(fckMPa + 8.0) * 1000.0;
    ecmSource = "grade '" + gradeName + "' is not in the EN1992-1-1 Table 3.1 lookup implemented here; "
                "derived from fck=" + std::to_string(fckMPa) + " MPa via the EN(RC) formula Ecm=9.5*(fck+8)^(1/3)*1e3 MPa instead";
  }

  ResolvedMaterial r;
  r.material.id = id;
  r.material.elastic_modulus = EcmMPa * 1000.0; // MPa -> kN/m^2
  r.material.poisson_ratio = 0.2;
  r.material.unit_weight = 25.0;
  r.material.thermal_expansion = 1.0e-5;
  r.assumptions = {
    {"elastic_modulus", std::to_string(r.material.elastic_modulus) + " kN/m^2", ecmSource},
    {"poisson_ratio", "0.2", "not present in the MIDAS export for a grade-named material; EN1992-1-1 S3.1.3 standard value for concrete"},
    {"unit_weight", "25 kN/m^3", "not present in the MIDAS export for a grade-named material; EN1991-1-1 standard value for normal-weight reinforced concrete"},
    {"thermal_expansion", "1.0e-5 /K", "not present in the MIDAS export for a grade-named material; EN1992-1-1 S3.1.3(5) standard value for normal-weight concrete"},
  };
  return r;
}

} // namespace kopruq::io::midas
