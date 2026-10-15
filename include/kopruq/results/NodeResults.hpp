#pragma once
#include "../core/Types.hpp"
namespace kopruq { struct NodeResults { Id node_id{}; DofVector displacement{}; DofVector reaction{}; std::array<bool,6> restrained{}; }; }
