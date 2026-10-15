#pragma once
#include "../core/Model.hpp"
#include "../solvers/SolverStatus.hpp"
namespace kopruq { inline constexpr const char* version="0.1.0"; class SolverAPI { public: SolverStatus validate(const Model& model) const { return {true, model.nodes.empty() ? "model is empty" : "model valid"}; } }; }
