#pragma once
#include "../core/Types.hpp"
#include <array>
namespace kopruq {
// reaction: global axes. reaction_local / local_axes (2026-11-21): for a node with local axes
// (Model::node_axes) the reaction in those axes, along which its restraint flags act; otherwise
// reaction_local == reaction and local_axes is false.
struct NodeResults { Id node_id{}; DofVector displacement{}; DofVector reaction{}; std::array<bool,6> restrained{}; DofVector reaction_local{}; bool local_axes{false}; };
}
