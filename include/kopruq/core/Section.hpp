#pragma once
#include "Types.hpp"
#include <cmath>
#include <stdexcept>
namespace kopruq { struct Section { Id id{}; Real area{}; Real Iy{}, Iz{}, J{}; void validate() const { if(id==0||!std::isfinite(area)||area<=0||!std::isfinite(Iy)||!std::isfinite(Iz)||!std::isfinite(J)||(Iy!=0&&Iy<=0)||(Iz!=0&&Iz<=0)||(J!=0&&J<=0)) throw std::invalid_argument("invalid section"); } }; }
