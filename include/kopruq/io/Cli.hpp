#pragma once
// Command-line front end, kept separate from main() so it can be tested in-process.
#include "ModalResultsJson.hpp"
#include "ModelViewJson.hpp"
#include "ResponseSpectrumResultsJson.hpp"
#include "ResultsJson.hpp"
#include "Sha256.hpp"
#include "Utf8Path.hpp"
#include "midas/MidasJsonImporter.hpp"
#include <chrono>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <vector>

namespace kopruq::io {

inline constexpr const char* cli_usage =
  "Usage: kopruq_solver_cli [--version|--help]\n"
  "       kopruq_solver_cli solve <model.json> [-o <results.json>] [--format <native|midas-nx-json>]\n"
  "       kopruq_solver_cli modal <model.json> [-o <results.json>] [-n <num-modes>] [--format <native|midas-nx-json>] [--mass-type <consistent|lumped>]\n"
  "       kopruq_solver_cli spectrum <model.json> [-o <results.json>] [--format <native|midas-nx-json>] [--mass-type <consistent|lumped>]\n"
  "                         (runs every \"response_spectra\" entry defined in the model)\n"
  "                         (solve, modal and spectrum accept --progress json: newline-delimited JSON events on stderr)\n"
  "       kopruq_solver_cli inspect <model.json> [-o <view.json>] [--format <native|midas-nx-json>]\n"
  "                         (writes the kopruq-model-view document for viewers; no analysis is run)\n"
  "\n"
  "--format midas-nx-json reads a MIDAS Civil NX JSON export instead of the native model\n"
  "schema (Phase 7B/7C importer; only the subset documented in\n"
  "docs/MIDAS_JSON_DATA_AUDIT.md is supported -- unsupported records are rejected, not\n"
  "guessed at, and every disclosed assumption/rejection is printed as a diagnostic).\n"
  "\n"
  "--mass-type (modal/spectrum only, default consistent): consistent is ModalSolver's own\n"
  "long-standing, independently-verified mass formulation; lumped is a classic translational-\n"
  "only lumped mass (zero rotational mass), added specifically because it reproduces MIDAS NX's\n"
  "own default eigenvalue analysis far more closely on the models it was validated against --\n"
  "see docs/SOLVER_ROADMAP.md's Phase 7H entry. Works with rigid links (slave masses move to the master).\n"
  "\n"
  "Exit codes: 0 success, 1 analysis failed (results still written),\n"
  "            2 usage error, 3 invalid model, 4 file I/O error.\n";

namespace detail {

struct CommonArgs { std::string modelPath, outputPath, format = "native"; int numModes = 6; bool haveNumModes = false; MassMatrixType massType = MassMatrixType::Consistent; bool progress = false; };

// Parses the shared "<model.json> [-o <out>] [-n <modes>] [--format <fmt>] [--mass-type <type>]"
// tail; -n/--mass-type are only meaningful for "modal"/"spectrum" but accepted here so every
// command shares one parser. Returns false (and writes to err) on a usage error.
inline bool parseArgs(const char* command, const std::vector<std::string>& args, std::ostream& err, CommonArgs& out) {
  for (std::size_t i = 1; i < args.size(); ++i) {
    if (args[i] == "-o" || args[i] == "--output") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs a file path\n"; return false; }
      out.outputPath = args[++i];
    } else if (args[i] == "-n" || args[i] == "--num-modes") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs a number\n"; return false; }
      try { out.numModes = std::stoi(args[++i]); out.haveNumModes = true; }
      catch (const std::exception&) { err << command << ": invalid number of modes '" << args[i] << "'\n"; return false; }
    } else if (args[i] == "--mass-type") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs 'consistent' or 'lumped'\n"; return false; }
      const std::string& v = args[++i];
      if (v == "consistent") out.massType = MassMatrixType::Consistent;
      else if (v == "lumped") out.massType = MassMatrixType::Lumped;
      else { err << command << ": unsupported --mass-type '" << v << "' (expected consistent or lumped)\n"; return false; }
    } else if (args[i] == "--progress") {
      if (i + 1 >= args.size() || args[i + 1] != "json") { err << command << ": --progress needs the value 'json'"<<'\n'; return false; }
      ++i; out.progress = true;
    } else if (args[i] == "--format") {
      if (i + 1 >= args.size()) { err << command << ": " << args[i] << " needs a format name\n"; return false; }
      out.format = args[++i];
      if (out.format != "native" && out.format != "midas-nx-json") {
        err << command << ": unsupported --format '" << out.format << "' (expected native or midas-nx-json)\n"; return false;
      }
    } else if (out.modelPath.empty()) {
      out.modelPath = args[i];
    } else {
      err << command << ": unexpected argument '" << args[i] << "'\n"; return false;
    }
  }
  if (out.modelPath.empty()) { err << command << ": missing model file\n" << cli_usage; return false; }
  return true;
}

// Prints every disclosed assumption/rejection from a MIDAS import so missing or unsupported
// data is always visible to the caller, never silently dropped (CLAUDE.md quality gate).
inline void reportMidasImport(const midas::MidasImportReport& r, std::ostream& err) {
  err << "midas-nx-json import: " << r.nodes << " nodes, " << r.beams << " beams, " << r.materials
      << " materials, " << r.sections << " sections, " << r.supports << " supports, "
      << r.rigid_links << " rigid links, " << r.load_cases << " load cases, " << r.nodal_loads
      << " nodal loads, " << r.member_loads << " member loads, " << r.thermal_loads
      << " thermal loads, " << r.springs << " elastic links, " << r.joint_springs
      << " joint springs, " << r.elastic_links_rejected << " elastic links rejected\n";
  if (r.hasBoundingBox) {
    err << "midas-nx-json bounds: X[" << r.minX << "," << r.maxX << "] Y[" << r.minY << "," << r.maxY
        << "] Z[" << r.minZ << "," << r.maxZ << "]\n";
  }
  for (const auto& a : r.assumptions) err << "  [assumed] " << a << '\n';
  for (const auto& rj : r.rejected) err << "  [rejected] " << rj << '\n';
}

// Reads the model file (native schema or, if format=="midas-nx-json", a MIDAS Civil NX JSON
// export via the Phase 7B importer), or returns false having written the exit-4/exit-3
// diagnostic to err (caller returns that code).
inline bool readModelFile(const std::string& path, const std::string& format, std::ostream& err, std::string& text, ModelInput& model, int& exitCode) {
  std::ifstream in(utf8Path(path), std::ios::binary); // path is UTF-8 (Utf8Path.hpp)
  if (!in) { err << "cannot open model file: " << path << '\n'; exitCode = 4; return false; }
  text.assign((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (format == "midas-nx-json") {
    try {
      const auto parsed = nlohmann::json::parse(text);
      const auto result = midas::importMidasJson(parsed);
      reportMidasImport(result.report, err);
      model = result.input;
    } catch (const std::exception& e) {
      err << "invalid MIDAS NX JSON model " << path << ": " << e.what() << '\n';
      exitCode = 3; return false;
    }
    return true;
  }
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
  std::ofstream o(utf8Path(outputPath), std::ios::binary); // path is UTF-8 (Utf8Path.hpp)
  o << document;
  if (!o) { err << "cannot write results file: " << outputPath << '\n'; return 4; }
  return 0;
}

// Progress events (2026-11-23, opt-in --progress json): newline-delimited JSON on stderr, e.g.
//   {"event":"stage","name":"read","state":"start","t":0.001}   stages: read, solve, write
//   {"event":"case","name":"SPX","index":1,"of":2,"t":0.4}        (spectrum: one per response-spectrum case)
//   {"event":"done","exit":0,"t":0.9}                              (always last, written by runCli)
// Without the flag nothing is emitted and every output is byte-identical to before.
inline std::chrono::steady_clock::time_point& progressClock() { static std::chrono::steady_clock::time_point t0 = std::chrono::steady_clock::now(); return t0; }
struct Progress {
  bool on; std::ostream& err;
  void send(const json& j) const { if (on) err << j.dump() << '\n'; }
  static double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now() - progressClock()).count(); }
  void stage(const char* name, const char* state) const { send({{"event", "stage"}, {"name", name}, {"state", state}, {"t", now()}}); }
  void item(const std::string& name, std::size_t index, std::size_t of) const { send({{"event", "case"}, {"name", name}, {"index", index}, {"of", of}, {"t", now()}}); }
};

inline int runSolve(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("solve", args, err, a)) return 2;
  const Progress pr{a.progress, err};
  std::string text; ModelInput model; int code;
  pr.stage("read", "start");
  if (!readModelFile(a.modelPath, a.format, err, text, model, code)) return code;
  pr.stage("read", "done"); pr.stage("solve", "start");
  const auto result = LinearStaticSolver::solveCases(model.model, model.restraints, model.load_cases, model.combinations);
  pr.stage("solve", "done"); pr.stage("write", "start");
  const std::string document = writeResults(model, result, sha256Hex(text)).dump(2) + "\n";
  if (int rc = writeDocument(a.outputPath, document, out, err)) return rc;
  pr.stage("write", "done");
  if (!result.status.success) { err << "analysis failed: " << result.status.message << '\n'; return 1; }
  return 0;
}

// inspect (2026-11-23): model -> kopruq-model-view v1 for viewers. Reads like `solve` (same exit
// codes: 2 usage, 3 invalid model, 4 I/O) but runs no analysis; for a MIDAS model the structured import
// report is included in the document instead of only being printed on stderr.
inline int runInspect(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("inspect", args, err, a)) return 2;
  std::ifstream in(utf8Path(a.modelPath), std::ios::binary);
  if (!in) { err << "cannot open model file: " << a.modelPath << '\n'; return 4; }
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  ModelInput model; midas::MidasImportResult imported; const bool isMidas = a.format == "midas-nx-json";
  try {
    if (isMidas) { imported = midas::importMidasJson(nlohmann::json::parse(text)); model = imported.input; }
    else model = readModelText(text);
  } catch (const ModelInputError& e) {
    err << "invalid model " << a.modelPath << " (" << e.issues.size() << " issue" << (e.issues.size() == 1 ? "" : "s") << "):\n";
    for (const auto& i : e.issues) err << "  " << i.path << ": " << i.message << '\n';
    return 3;
  } catch (const std::exception& e) { err << "invalid model " << a.modelPath << ": " << e.what() << '\n'; return 3; }
  const std::string document = modelViewJson(model, isMidas ? &imported.report : nullptr, a.format, sha256Hex(text)).dump(2) + "\n";
  return writeDocument(a.outputPath, document, out, err);
}

inline int runModal(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("modal", args, err, a)) return 2;
  if (a.haveNumModes && a.numModes <= 0) { err << "modal: -n must be a positive integer\n"; return 2; }
  const Progress pr{a.progress, err};
  std::string text; ModelInput model; int code;
  pr.stage("read", "start");
  if (!readModelFile(a.modelPath, a.format, err, text, model, code)) return code;
  pr.stage("read", "done");
  // -n on the command line always wins; otherwise fall back to the model's own modal request
  // (e.g. MIDAS EIGV's iFREQ), if the source format populated one, before this command's own
  // default (6) -- never changes behavior for a model/format with no such request.
  const int numModes = a.haveNumModes ? a.numModes : (model.modal_request.present ? model.modal_request.num_modes : a.numModes);
  pr.stage("solve", "start");
  const auto result = ModalSolver::solve(model.model, model.restraints, numModes, a.massType);
  pr.stage("solve", "done"); pr.stage("write", "start");
  const std::string document = writeModalResults(model.model, result, sha256Hex(text)).dump(2) + "\n";
  if (int rc = writeDocument(a.outputPath, document, out, err)) return rc;
  pr.stage("write", "done");
  if (!result.status.success) { err << "analysis failed: " << result.status.message << '\n'; return 1; }
  return 0;
}

inline int runSpectrum(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  CommonArgs a; if (!parseArgs("spectrum", args, err, a)) return 2;
  const Progress pr{a.progress, err};
  std::string text; ModelInput model; int code;
  pr.stage("read", "start");
  if (!readModelFile(a.modelPath, a.format, err, text, model, code)) return code;
  pr.stage("read", "done"); pr.stage("solve", "start");
  std::vector<std::pair<std::string, ResponseSpectrumResults>> results;
  results.reserve(model.response_spectra.size());
  for (const auto& rc : model.response_spectra) {
    pr.item(rc.name, results.size() + 1, model.response_spectra.size());
    results.emplace_back(rc.name, ResponseSpectrumSolver::solve(model.model, model.restraints, rc.input, a.massType));
  }
  pr.stage("solve", "done"); pr.stage("write", "start");
  const json document_json = writeResponseSpectrumResults(results, sha256Hex(text));
  const std::string document = document_json.dump(2) + "\n";
  if (int rc = writeDocument(a.outputPath, document, out, err)) return rc;
  pr.stage("write", "done");
  if (!document_json["status"]["success"].get<bool>()) { err << "analysis failed: " << document_json["status"]["message"].get<std::string>() << '\n'; return 1; }
  return 0;
}

} // namespace detail

inline int runCli(const std::vector<std::string>& args, std::ostream& out, std::ostream& err) {
  detail::progressClock(); // start the clock of the progress events
  int rc = 2;
  const std::string command = args.empty() ? "--help" : args[0];
  if (command == "--version" || command == "version") { out << "KOPRUQ-SOLVER " << version << '\n'; return 0; }
  if (command == "--help" || command == "help") { out << cli_usage; return 0; }
  if (command == "solve") rc = detail::runSolve(args, out, err);
  else if (command == "inspect") rc = detail::runInspect(args, out, err);
  else if (command == "spectrum") rc = detail::runSpectrum(args, out, err);
  else if (command == "modal") rc = detail::runModal(args, out, err);
  else { err << "Unknown command: " << command << '\n'; return 2; }
  // the closing event of an opt-in progress stream (also for failures: the exit code says which)
  for (std::size_t i = 1; i + 1 < args.size(); ++i)
    if (args[i] == "--progress" && args[i + 1] == "json") { detail::Progress{true, err}.send({{"event", "done"}, {"exit", rc}, {"t", detail::Progress::now()}}); break; }
  return rc;
}

} // namespace kopruq::io
