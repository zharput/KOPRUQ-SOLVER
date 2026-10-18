#pragma once
#include "Types.hpp"
#include <cmath>
#include <stdexcept>
// Asy/Asz: effective shear area [m^2] for the y-bending (UY,RZ, uses Iz) and z-bending (UZ,RY, uses Iy) planes,
// used only for the Timoshenko shear-deformation correction (kopruq/elements/Beam3D.hpp). 0 (default) means
// "rigid in shear" -- Euler-Bernoulli, the prior behavior, exactly recovered since the Timoshenko shear
// parameter Phi is defined as 0 in that case.
namespace kopruq { struct Section { Id id{}; Real area{}; Real Iy{}, Iz{}, J{}; Real Asy{0}, Asz{0}; void validate() const { if(id==0||!std::isfinite(area)||area<=0||!std::isfinite(Iy)||!std::isfinite(Iz)||!std::isfinite(J)||(Iy!=0&&Iy<=0)||(Iz!=0&&Iz<=0)||(J!=0&&J<=0)||!std::isfinite(Asy)||Asy<0||!std::isfinite(Asz)||Asz<0) throw std::invalid_argument("invalid section"); } }; }
