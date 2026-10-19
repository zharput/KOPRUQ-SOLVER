#pragma once
#include "Types.hpp"
#include <cmath>
#include <stdexcept>
// unit_weight: weight per unit volume in kN/m^3 (e.g. ~78.5 for steel, ~25 for reinforced concrete); used only by self-weight loads. 0 = weightless.
// thermal_expansion: coefficient of thermal expansion alpha_T [1/K] (e.g. ~1.0e-5 for concrete per EN1992-1-1,
// ~1.2e-5 for structural steel); used only by thermal loads (kopruq/loads/ThermalLoads.hpp). 0 = no thermal response.
namespace kopruq { struct Material { Id id{}; Real elastic_modulus{}; Real poisson_ratio{}; Real unit_weight{}; Real thermal_expansion{0}; void validate() const { if(id==0||!std::isfinite(elastic_modulus)||!std::isfinite(poisson_ratio)||elastic_modulus<=0||poisson_ratio<=-1||poisson_ratio>=0.5||!std::isfinite(unit_weight)||unit_weight<0||!std::isfinite(thermal_expansion)) throw std::invalid_argument("invalid material"); } }; }
