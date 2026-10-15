#pragma once
#include <string>
namespace kopruq { enum class SolverStatusCode { Success, InvalidModel, NoRestraints, SingularSystem, NonFiniteSolution, NumericalFailure }; struct SolverStatus { bool success{}; std::string message; SolverStatusCode code{SolverStatusCode::NumericalFailure}; }; }
