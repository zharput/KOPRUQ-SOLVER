#pragma once
#include "Types.hpp"
#include <cmath>
#include <stdexcept>
// unit_weight: weight per unit volume in kN/m^3 (e.g. ~78.5 for steel, ~25 for reinforced concrete); used only by self-weight loads. 0 = weightless.
namespace kopruq { struct Material { Id id{}; Real elastic_modulus{}; Real poisson_ratio{}; Real unit_weight{}; void validate() const { if(id==0||!std::isfinite(elastic_modulus)||!std::isfinite(poisson_ratio)||elastic_modulus<=0||poisson_ratio<=-1||poisson_ratio>=0.5||!std::isfinite(unit_weight)||unit_weight<0) throw std::invalid_argument("invalid material"); } }; }
