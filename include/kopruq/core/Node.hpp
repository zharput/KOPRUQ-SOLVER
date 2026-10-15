#pragma once
#include "Types.hpp"
#include <stdexcept>
namespace kopruq {
struct Node { Id id{}; Real x{}, y{}, z{}; DofVector loads{}; Node()=default; Node(Id i, Real x_, Real y_, Real z_):id(i),x(x_),y(y_),z(z_) { if(i==0) throw std::invalid_argument("node id must be non-zero"); } };
}
