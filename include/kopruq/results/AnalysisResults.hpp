#pragma once
#include "NodeResults.hpp"
#include "ElementResults.hpp"
#include <Eigen/Dense>
#include <vector>
// element_equilibrium_error = sum_e ||f1+f2|| / sum_e (||f1||+||f2||) over all elements (force-weighted, so never above
// max_element_force_error); element_moment_equilibrium_error is the same ratio using each element's moment balance and scale.
namespace kopruq { struct AnalysisResults { std::vector<NodeResults> nodes; std::vector<ElementResults> elements; Eigen::Vector3d force_balance=Eigen::Vector3d::Zero(),moment_balance=Eigen::Vector3d::Zero(); Real force_balance_norm{},moment_balance_norm{},force_equilibrium_error{},moment_equilibrium_error{}; Real element_equilibrium_error{},element_moment_equilibrium_error{},max_element_force_error{},max_element_moment_error{}; Id max_force_element_id{},max_moment_element_id{}; Real strain_energy{},element_strain_energy{},energy_error{}; bool valid{}; }; }
