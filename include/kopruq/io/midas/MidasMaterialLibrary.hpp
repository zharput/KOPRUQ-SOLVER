#pragma once
// Resolves a MIDAS Civil NX MATL/MATD "design-code grade" material (e.g.
// "C35/45" under code family "EN04(RC)") into KOPRUQ-SOLVER's Material
// (elastic_modulus, poisson_ratio, unit_weight). The MIDAS JSON export
// gives a grade name and characteristic strength, never E/Poisson/density
// directly -- see docs/MIDAS_JSON_DATA_AUDIT.md section 4.2.
//
// Elastic modulus is derived from the characteristic compressive strength
// via the formula MIDAS documents for its EN(RC) material family
// (support.midasuser.com, "Modify Concrete Materials"):
//   Ecm [MPa] = 9.5 * (fck + 8)^(1/3) * 10^3,  fck in MPa
// converted to kN/m^2 (this project's canonical unit) by multiplying by
// 1000 (1 MPa = 1000 kN/m^2). This is MIDAS's own documented formula for
// this specific code family, not a general engineering derivation, and is
// used only to reproduce what MIDAS itself would have computed -- only
// code names containing "EN" and "(RC)" are accepted; anything else is
// rejected with a clear diagnostic rather than guessed at.
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
#include <stdexcept>
#include <string>
#include <vector>

namespace kopruq::io::midas {

// One disclosed substitution: which field, what value was used, and why -- so a
// reconstruction report can print "assumed poisson_ratio=0.2 (EN1992-1-1 S3.1.3)"
// instead of silently using the number.
struct MaterialAssumption { std::string field, value, source; };

struct ResolvedMaterial { kopruq::Material material; std::vector<MaterialAssumption> assumptions; };

// codeName: MATD's DATA1.CODENAME (e.g. "EN04(RC)"). fckMPa: MATD's DATA1.DESIGN.C_FC,
// already in MPa (the export's C_FC is in kN/m^2 = 1000x the MPa value; caller divides).
inline ResolvedMaterial resolveConcreteGradeMaterial(kopruq::Id id, const std::string& codeName, double fckMPa) {
  if (codeName.find("EN") == std::string::npos || codeName.find("(RC)") == std::string::npos)
    throw std::invalid_argument("unsupported material design code '" + codeName + "': only the EN..(RC) family's E-fck formula is implemented (see MidasMaterialLibrary.hpp)");
  if (!std::isfinite(fckMPa) || fckMPa <= 0)
    throw std::invalid_argument("material characteristic strength must be finite and positive");
  const double EcmMPa = 9.5 * std::cbrt(fckMPa + 8.0) * 1000.0;
  ResolvedMaterial r;
  r.material.id = id;
  r.material.elastic_modulus = EcmMPa * 1000.0; // MPa -> kN/m^2
  r.material.poisson_ratio = 0.2;
  r.material.unit_weight = 25.0;
  r.material.thermal_expansion = 1.0e-5;
  r.assumptions = {
    {"elastic_modulus", std::to_string(r.material.elastic_modulus) + " kN/m^2",
     "derived from fck=" + std::to_string(fckMPa) + " MPa via MIDAS's documented EN(RC) formula Ecm=9.5*(fck+8)^(1/3)*1e3 MPa (support.midasuser.com, 'Modify Concrete Materials')"},
    {"poisson_ratio", "0.2", "not present in the MIDAS export for a grade-named material; EN1992-1-1 S3.1.3 standard value for concrete"},
    {"unit_weight", "25 kN/m^3", "not present in the MIDAS export for a grade-named material; EN1991-1-1 standard value for normal-weight reinforced concrete"},
    {"thermal_expansion", "1.0e-5 /K", "not present in the MIDAS export for a grade-named material; EN1992-1-1 S3.1.3(5) standard value for normal-weight concrete"},
  };
  return r;
}

} // namespace kopruq::io::midas
