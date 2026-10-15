#pragma once
#include "Types.hpp"
#include <cmath>
#include <stdexcept>
namespace kopruq { struct Material { Id id{}; Real elastic_modulus{}; Real poisson_ratio{}; void validate() const { if(id==0||!std::isfinite(elastic_modulus)||!std::isfinite(poisson_ratio)||elastic_modulus<=0||poisson_ratio<=-1||poisson_ratio>=0.5) throw std::invalid_argument("invalid material"); } }; }
