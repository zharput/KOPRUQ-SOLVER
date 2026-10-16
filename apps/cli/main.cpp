#include <iostream>
#include <string>
#include <vector>
#include "kopruq/io/Cli.hpp"
int main(int argc, char** argv) { std::vector<std::string> args(argv + 1, argv + argc); return kopruq::io::runCli(args, std::cout, std::cerr); }
