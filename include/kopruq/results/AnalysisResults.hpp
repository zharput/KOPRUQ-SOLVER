#pragma once
#include "NodeResults.hpp"
#include "ElementResults.hpp"
#include <Eigen/Dense>
#include <vector>
// element_equilibrium_error = sum_e ||f1+f2|| / sum_e (||f1||+||f2||) over all elements (force-weighted, so never above
// max_element_force_error); element_moment_equilibrium_error is the same ratio using each element's moment balance and scale.
// springs: per-spring end forces/energy (kopruq/elements/SpringLink.hpp), reported separately from `elements` (beams)
// since the two element kinds use independent id spaces; included in element_strain_energy/energy_error regardless.
// joint_springs: per-joint-spring force/energy (kopruq/elements/JointSpring.hpp) -- only indices 0..5 of
// local_end_force/global_end_force are meaningful (the single node's own 6 DOFs; there is no "other end",
// so indices 6..11 and force_balance/moment_balance/normalized_*_error are left at their default zero rather
// than fabricating a per-element balance check that doesn't apply to a one-node element). global_end_force is
// "force exerted by the node on the spring" (same convention as elements'/springs' end forces); its negative
// (the ground's reaction through the spring, Newton's third law) is folded into the model-level
// force_balance/moment_balance for any DOF component NOT already covered by an explicit restraint at that
// node (see LinearStaticSolver.hpp's recoverAssembled): a joint spring is a flexible connection to the
// (external) ground, exactly like a support, so its reaction is part of the model's external equilibrium --
// unlike a two-node spring (`springs`), whose force is purely internal between two nodes that are both
// already part of the model.
namespace kopruq { struct AnalysisResults { std::vector<NodeResults> nodes; std::vector<ElementResults> elements; std::vector<ElementResults> springs; std::vector<ElementResults> joint_springs; Eigen::Vector3d force_balance=Eigen::Vector3d::Zero(),moment_balance=Eigen::Vector3d::Zero(); Real force_balance_norm{},moment_balance_norm{},force_equilibrium_error{},moment_equilibrium_error{}; Real element_equilibrium_error{},element_moment_equilibrium_error{},max_element_force_error{},max_element_moment_error{}; Id max_force_element_id{},max_moment_element_id{}; Real strain_energy{},element_strain_energy{},energy_error{}; bool valid{}; }; }
