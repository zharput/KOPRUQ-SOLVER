#pragma once
// Command-line front end, kept separate from main() so it can be tested in-process.
#include "ModalResultsJson.hpp"
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
  "       kopruq_solver_cli modal <model.json> [-o <results.json>] [-n <num-modes>]\n"
  "\n"
  "Exit codes: 0 success, 1 analysis failed (results still written),\n"
  "            2 usage error, 3 invalid model, 4 file I/O error.\n";

namespace detail {

struct CommonArgs { std::string modelPath, outputPath; int numModes = 6; bool haveNumModes = false; };

// Parses the shared "<model.json> [-o <out>] [-n <modes>]" tail; -n is only meaningful for "modal"
// but accepted here so both commands share one parser. Returns false (and writes to err) on a usage error.
inline bool parseArgs(const char* command, const std::vector<std::string>& args, std::ostream& err, CommonArgs& out) {
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-o" || args[i] == "--output") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs a file path\n"; return false; }
      out.outputPath = args[++i];
    } else if (args[i] == "-n" || args[i] == "--num-modes") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs a number\n"; return false; }
      try { out.numModes = std::stoi(args[++i]); out.haveNumModes = true; }
      catch (const std::exception&) { err << command << ": invalid number of modes '" << args[i] << "'\n"; return false; }
    } else if (out.modelPath.empty()) {
      out.modelPath = args[i];
    } else {
      err << command << ": unexpected argument '" << args[i] << "'\n"; return false;
    }
  }
  if (out.modelPath.empty()) { err << command << ": missing model file\n" << cli_usage; return false; }
  return true;
}

// Reads the model file, or returns false having written the exit-4/exit-3 diagnostic to err (caller returns that code).
inline bool readModelFile(const std::string& path, std::ostream& err, std::string& text, ModelInput& model, int& exitCode) {
  std::ifstream in(path, std::ios::binary);
  if (!in) { err << "cannot open model file: " << path << '\n'; exitCode = 4; return false; }
  text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  try { model = readModelText(text); }
  catch (const ModelInputError& e) {
    err << "invalid model " << path << " (" << e.issues.size() << " issue" << (e.issues.size() == 1 ? "" : "s") << "):\n";
    for (const auto& i : e.issues) err << "  " << i.path << ": " << i.message << '\n';
    exitCode = 3; return false;
  }
  return true;
}

inline int writeDocument(const std::string& outputPath, const std::string& document, std::ostream& out, std::ostream& err) {
  if (outputPath.empty()) { out << document; return 0; }
  std::ofstream o(outputPath, std::ios::binary);
  o << document;
  if (!o) { err << "cannot write results file: " << outputPath << '\n'; return 4; }
  return 0;
}

inline int runSolve(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("solve", args, err, a)) return 2;
  std::string text; ModelInput model; int code;
  if (!readModelFile(a.modelPath, err, text, model, code)) return code;
  const auto result = LinearStaticSolver::solveCases(model.model, model.restraints, model.load_cases, model.combinations);
  const std::string document = writeResults(model, result, sha256Hex(text)).dump(2) + "\n";
  if (int rc = writeDocument(a.outputPath, document, out, err)) return rc;
  if (!result.status.success) { err << "analysis failed: " << result.status.message << '\n'; return 1; }
  return 0;
}

inline int runModal(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("modal", args, err, a)) return 2;
  if (a.haveNumModes && a.numModes <= 0) { err << "modal: -n must be a positive integer\n"; return 2; }
  std::string text; ModelInput model; int code;
  if (!readModelFile(a.modelPath, err, text, model, code)) return code;
  const auto result = ModalSolver::solve(model.model, model.restraints, a.numModes);
  const std::string document = writeModalResults(model.model, result, sha256Hex(text)).dump(2) + "\n";
  if (int rc = writeDocument(a.outputPath, document, out, err)) return rc;
  if (!result.status.success) { err << "analysis failed: " << result.status.message << '\n'; return 1; }
  return 0;
}

} // namespace detail

inline int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  const std::string command = args.empty() ? "--help" : args[0];
  if (command == "--version" || command == "version") { out << "KOPRUQ-SOLVER " << version << '\n'; return 0; }
  if (command == "--help" || command == "help") { out << cli_usage; return 0; }
  if (command == "solve") return detail::runSolve(args, out, err);
  if (command == "modal") return detail::runModal(args, out, err);
  err << "Unknown command: " << command << '\n'; return 2;
}

} // namespace kopruq::io
