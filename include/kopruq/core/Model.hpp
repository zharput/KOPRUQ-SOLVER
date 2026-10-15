#pragma once
#include "Node.hpp"
#include "Material.hpp"
#include "Section.hpp"
#include <Eigen/Dense>
#include <vector>
namespace kopruq { struct Beam3DConnection { Id id{}, node1{}, node2{}, material{}, section{}; Eigen::Vector3d reference_y{0,1,0}; }; struct Model { std::vector<Node> nodes; std::vector<Material> materials; std::vector<Section> sections; std::vector<Beam3DConnection> beams; }; }
