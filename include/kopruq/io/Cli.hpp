#pragma once
// Command-line front end, kept separate from main() so it can be tested in-process.
#include "ResultsJson.hpp"
#include "Sha256.hpp"
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

namespace kopruq::io {

inline constexpr const char* cli_usage =
  "Usage: kopruq_solver_cli [--version|--help]\n"
  "       kopruq_solver_cli solve <model.json> [-o <results.json>]\n"
  "\n"
  "Exit codes: 0 success, 1 analysis failed (results still written),\n"
  "            2 usage error, 3 invalid model, 4 file I/O error.\n";

inline int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  const std::string command = args.empty() ? "--help" : args[0];
  if (command == "--version" || command == "version") { out << "KOPRUQ-SOLVER " << version << '\n'; return 0; }
  if (command == "--help" || command == "help") { out << cli_usage; return 0; }
  if (command != "solve") { err << "Unknown command: " << command << '\n'; return 2; }

  std::string modelPath, outputPath;
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-o" || args[i] == "--output") {
      if (i + 1 >= args.size()) { err << "solve: " << args[i] << " needs a file path\n"; return 2; }
      outputPath = args[++i];
    } else if (modelPath.empty()) {
      modelPath = args[i];
    } else {
      err << "solve: unexpected argument '" << args[i] << "'\n"; return 2;
    }
  }
  if (modelPath.empty()) { err << "solve: missing model file\n" << cli_usage; return 2; }

  std::ifstream in(modelPath, std::ios::binary);
  if (!in) { err << "cannot open model file: " << modelPath << '\n'; return 4; }
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  ModelInput model;
  try { model = readModelText(text); }
  catch (const ModelInputError& e) {
    err << "invalid model " << modelPath << " (" << e.issues.size() << " issue" << (e.issues.size() == 1 ? "" : "s") << "):\n";
    for (const auto& i : e.issues) err << "  " << i.path << ": " << i.message << '\n';
    return 3;
  }

  const auto result = LinearStaticSolver::solveCases(model.model, model.restraints, model.load_cases, model.combinations);
  const std::string document = writeResults(model, result, sha256Hex(text)).dump(2) + "\n";
  if (outputPath.empty()) {
    out << document;
  } else {
    std::ofstream o(outputPath, std::ios::binary);
    o << document;
    if (!o) { err << "cannot write results file: " << outputPath << '\n'; return 4; }
  }
  if (!result.status.success) { err << "analysis failed: " << result.status.message << '\n'; return 1; }
  return 0;
}

} // namespace kopruq::io
