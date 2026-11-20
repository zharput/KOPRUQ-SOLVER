#pragma once
// Resolves a MIDAS Civil NX MATL/MATD "design-code grade" material (e.g.
// "C35/45" under code family "EN04(RC)") into KOPRUQ-SOLVER's Material
// (elastic_modulus, poisson_ratio, unit_weight). The MIDAS JSON export
// gives a grade name and characteristic strength, never E/Poisson/density
// directly -- see docs/MIDAS_JSON_DATA_AUDIT.md section 4.2.
//
// Elastic modulus (user decision, 2026-11-20): always the EN1992-1-1:2004 Table 3.1 formula
//   Ecm = 22 * (fcm/10)^0.3 GPa,  fcm = fck + 8 MPa
// for every grade -- NOT the table's rounded per-grade values (e.g. C35/45: 34077.1 MPa, not
// 34000 MPa). This is the same formula as kopruq/materials/EurocodeMaterialLibrary.hpp.
// Evidence (verification models 18 and 21): MIDAS's own displacement + beam-force exports,
// back-calculated per element (pure axial EA = N*L/du, bending EI from end moments/rotations),
// imply E = 34077 MPa for C35/45 to 5-7 significant digits on every section -- i.e. this formula
// (MIDAS shows it rounded to 1 MPa, a 4e-6 relative difference). History, superseded: until
// 2026-11-20 named grades used the table's rounded value (34000 MPa) and other grades the old
// ENV-1992 formula 9.5*(fck+8)^(1/3) GPa (33.28 GPa for C35/45) -- a DIFFERENT curve, not an
// equivalent form of 22*(fcm/10)^0.3 as an earlier comment here wrongly stated; the model-04
// deflection evidence used for that choice was ambiguous because of shear deformation.
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
// gradeName: MATD's DATA1.CODEMATLNAME (e.g. "C35/45"), used only in the returned
// assumption's diagnostic text; Ecm depends on fckMPa alone.
inline ResolvedMaterial resolveConcreteGradeMaterial(kopruq::Id id, const std::string& codeName, double fckMPa, const std::string& gradeName = {}) {
  if (codeName.find("EN") == std::string::npos || codeName.find("(RC)") == std::string::npos)
    throw std::invalid_argument("unsupported material design code '" + codeName + "': only the EN..(RC) family's E-fck formula is implemented (see MidasMaterialLibrary.hpp)");
  if (!std::isfinite(fckMPa) || fckMPa <= 0)
    throw std::invalid_argument("material characteristic strength must be finite and positive");

  // EN1992-1-1:2004 Table 3.1: Ecm = 22*(fcm/10)^0.3 GPa = 22000*(fcm/10)^0.3 MPa, fcm = fck+8.
  const double fcmMPa = fckMPa + 8.0;
  const double EcmMPa = 22000.0 * std::pow(fcmMPa / 10.0, 0.3);
  const std::string ecmSource = "grade '" + gradeName + "': derived from fck=" + std::to_string(fckMPa) +
      " MPa via EN1992-1-1:2004 Table 3.1 Ecm=22*(fcm/10)^0.3 GPa, fcm=fck+8 (formula, not the table's rounded value; "
      "see MidasMaterialLibrary.hpp header)";

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

// codeName: MATD's DATA1.CODENAME for a STEEL material (e.g. "EN05(S)" for a Eurocode 3 grade).
// gradeName: MATD's DATA1.CODEMATLNAME (e.g. "S275", "S355") -- used only for the returned
// assumption's diagnostic text; the elastic properties below do not depend on the grade, per
// EN1993-1-1 S3.2.6: structural steel's E, G (via Poisson) and density are the SAME fixed
// constants for every named grade (the grade only changes yield/ultimate strength, fy/fu, which
// this linear-elastic solver does not use). Only code names containing "EN" and "(S)" are
// accepted; anything else is rejected rather than guessed at, mirroring
// resolveConcreteGradeMaterial's own code-family gating above.
inline ResolvedMaterial resolveSteelGradeMaterial(kopruq::Id id, const std::string& codeName, const std::string& gradeName = {}) {
  if (codeName.find("EN") == std::string::npos || codeName.find("(S)") == std::string::npos)
    throw std::invalid_argument("unsupported material design code '" + codeName + "': only the EN..(S) structural-steel family is implemented (see MidasMaterialLibrary.hpp)");

  ResolvedMaterial r;
  r.material.id = id;
  r.material.elastic_modulus = 210000.0 * 1000.0; // EN1993-1-1 S3.2.6: E = 210000 MPa -> kN/m^2
  r.material.poisson_ratio = 0.3;                  // EN1993-1-1 S3.2.6
  r.material.unit_weight = 78.5;                   // EN1991-1-1 Table A.4: steel unit weight
  r.material.thermal_expansion = 1.2e-5;           // EN1993-1-1 S3.2.6 (T <= 100 C)
  r.assumptions = {
    {"elastic_modulus", std::to_string(r.material.elastic_modulus) + " kN/m^2",
     "not present in the MIDAS export for a grade-named steel material '" + gradeName + "'; EN1993-1-1 S3.2.6 fixed value (same for every EN steel grade)"},
    {"poisson_ratio", "0.3", "not present in the MIDAS export for a grade-named material; EN1993-1-1 S3.2.6 standard value for structural steel"},
    {"unit_weight", "78.5 kN/m^3", "not present in the MIDAS export for a grade-named material; EN1991-1-1 Table A.4 standard value for steel"},
    {"thermal_expansion", "1.2e-5 /K", "not present in the MIDAS export for a grade-named material; EN1993-1-1 S3.2.6 standard value for structural steel (T<=100C)"},
  };
  return r;
}

} // namespace kopruq::io::midas
