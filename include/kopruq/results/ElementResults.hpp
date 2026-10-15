#pragma once
#include "../core/Types.hpp"
#include <Eigen/Dense>
namespace kopruq { struct ElementResults { Id element_id{}; std::array<Real,12> local_end_force{}; std::array<Real,12> global_end_force{}; Eigen::Vector3d force_balance=Eigen::Vector3d::Zero(),moment_balance=Eigen::Vector3d::Zero(); Real force_balance_norm{},moment_balance_norm{},normalized_force_error{},normalized_moment_error{}; Real strain_energy{}; }; }
