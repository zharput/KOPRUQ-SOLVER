#pragma once
#include "Node.hpp"
#include "Material.hpp"
#include "Section.hpp"
#include "../elements/SpringLink.hpp"
#include "../elements/JointSpring.hpp"
#include <Eigen/Dense>
#include <array>
#include <map>
#include <vector>
// releases: end releases per member-local DOF, [0..5] = UX..RZ at node1, [6..11] at node2 (true = released, no end force).
// offset1/offset2: rigid end offsets (kopruq/elements/Offsets.hpp), global-axis vector from node1/node2 to the
// member's actual end point; {0,0,0} (default) is a no-op, preserving prior results for every model that omits them.
namespace kopruq { struct Beam3DConnection { Id id{}, node1{}, node2{}, material{}, section{}; Eigen::Vector3d reference_y{0,1,0}; std::array<bool,12> releases{}; Eigen::Vector3d offset1{0,0,0}, offset2{0,0,0}; };
// springs: discrete two-node ElasticLink elements (kopruq/elements/SpringLink.hpp; MIDAS "ELNK" General
// link), id space independent of beams'. joint_springs: single-node-to-ground elastic supports
// (kopruq/elements/JointSpring.hpp; MIDAS "NSPR" Nodal Spring), id space independent of both beams' and
// springs'. These are two deliberately separate concepts (two-node flexible link vs. one-node elastic
// support), not variations of one element -- see each header's own comment.
// rigid_links: master-slave multi-point constraints (kopruq/assembly/RigidLink.hpp); a slave's 6 DOFs are
// determined by its master's via rigid-body kinematics (all six, or the subset in RigidLink::dofs).
// dofs (2026-11-21): which slave DOFs (UX..RZ, global) are tied to the master; default all six. An untied
// slave DOF stays an independent DOF (it may be restrained or carry stiffness of its own); restraining a
// TIED slave DOF is still an error.
struct RigidLink { Id id{}, master{}, slave{}; std::array<bool,6> dofs{true,true,true,true,true,true}; };
// node_axes (2026-11-21, MIDAS "Node Local Axis" / SKEW): per-node local axes, a rotation matrix whose ROWS
// are the node's local x, y, z axes in global coordinates. For such a node the restraint flags act along
// its local axes and its reaction is also reported in them (NodeResults::reaction_local); displacements
// stay global. Handled by the same DOF-transformation layer as rigid links (kopruq/assembly/RigidLink.hpp).
struct Model { std::vector<Node> nodes; std::vector<Material> materials; std::vector<Section> sections; std::vector<Beam3DConnection> beams; std::vector<SpringConnection> springs; std::vector<JointSpringConnection> joint_springs; std::vector<RigidLink> rigid_links; std::map<Id, Eigen::Matrix3d> node_axes; };
// Restraint flags per node, UX..RZ; a map naturally forbids more than one entry per node.
using Restraints = std::map<Id, std::array<bool,6>>; }
