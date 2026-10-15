#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace kopruq {
using Real = double;
using Id = std::uint64_t;
enum class Dof : std::size_t { UX=0, UY, UZ, RX, RY, RZ };
constexpr std::size_t kDofPerNode = 6;
using DofVector = std::array<Real, kDofPerNode>;
}
