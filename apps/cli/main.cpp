#include <iostream>
#include <string>
#include "kopruq/api/SolverAPI.hpp"
int main(int argc, char** argv) { const std::string command = argc > 1 ? argv[1] : "--help"; if(command=="--version"||command=="version") { std::cout << "KOPRUQ-SOLVER " << kopruq::version << '\n'; return 0; } if(command=="--help"||command=="help") { std::cout << "Usage: kopruq_solver_cli [--version|--help]\n"; return 0; } std::cerr << "Unknown command: " << command << '\n'; return 2; }
