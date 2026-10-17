#pragma once
#include "Node.hpp"
#include "Material.hpp"
#include "Section.hpp"
#include <Eigen/Dense>
#include <array>
#include <map>
#include <vector>
// releases: end releases per member-local DOF, [0..5] = UX..RZ at node1, [6..11] at node2 (true = released, no end force).
// offset1/offset2: rigid end offsets (kopruq/elements/Offsets.hpp), global-axis vector from node1/node2 to the
// member's actual end point; {0,0,0} (default) is a no-op, preserving prior results for every model that omits them.
namespace kopruq { struct Beam3DConnection { Id id{}, node1{}, node2{}, material{}, section{}; Eigen::Vector3d reference_y{0,1,0}; std::array<bool,12> releases{}; Eigen::Vector3d offset1{0,0,0}, offset2{0,0,0}; }; struct Model { std::vector<Node> nodes; std::vector<Material> materials; std::vector<Section> sections; std::vector<Beam3DConnection> beams; };
// Restraint flags per node, UX..RZ; a map naturally forbids more than one entry per node.
using Restraints = std::map<Id, std::array<bool,6>>; }
