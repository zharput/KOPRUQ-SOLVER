// 2026-11-23 -- UTF-8 file paths (kopruq/io/Utf8Path.hpp). Verification model 100 lives in
// "100 Basit köprü": on Windows a narrow std::ifstream reads its path in the ANSI code page, so the
// GUI (which passes QString::toStdString(), i.e. UTF-8) could not open it and showed no model.
// Dedicated new file: model read and a full CLI solve through a folder named with Turkish letters
// (ö ü ş ğ ı -- the last three are not in code page 1252).
#include <gtest/gtest.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include "kopruq/io/Cli.hpp"

namespace {
// "kopruq_utf8_öü_şğı" as UTF-8 bytes (escaped so the source encoding does not matter).
const std::string kDirUtf8 = "kopruq_utf8_\xC3\xB6\xC3\xBC_\xC5\x9F\xC4\x9F\xC4\xB1";
const char* kModel = R"({
  "schema":"kopruq-model","schema_version":1,
  "nodes":[{"id":1,"x":0,"y":0,"z":0},{"id":2,"x":4,"y":0,"z":0}],
  "materials":[{"id":1,"elastic_modulus":2e8,"poisson_ratio":0.3}],
  "sections":[{"id":1,"area":0.02,"Iy":1e-4,"Iz":2e-4,"J":3e-4}],
  "beams":[{"id":7,"nodes":[1,2],"material":1,"section":1}],
  "supports":[{"node":1,"restraints":[true,true,true,true,true,true]}],
  "load_cases":[{"name":"c","nodal":[{"node":2,"values":[0,0,-1000,0,0,0]}]}]})";
struct TempDir {
  std::string utf8;  // the directory as a UTF-8 string, as the GUI/CLI pass it around
  std::filesystem::path path;
  TempDir() {
    const auto base = std::filesystem::temp_directory_path();
    path = base / kopruq::io::utf8Path(kDirUtf8);
    std::filesystem::create_directories(path);
    const auto u8 = path.u8string();
    utf8.assign(u8.begin(), u8.end());
  }
  ~TempDir() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}

TEST(Utf8Path, ModelFileInNonAsciiFolderIsRead) {
  TempDir dir;
  { std::ofstream(dir.path / "model.json", std::ios::binary) << kModel; }
  std::string text; kopruq::io::ModelInput model; int ec = 0; std::ostringstream err;
  ASSERT_TRUE(kopruq::io::detail::readModelFile(dir.utf8 + "/model.json", "native", err, text, model, ec)) << err.str();
  EXPECT_EQ(model.model.nodes.size(), 2u);
}

TEST(Utf8Path, CliSolveReadsAndWritesNonAsciiPaths) {
  TempDir dir;
  { std::ofstream(dir.path / "model.json", std::ios::binary) << kModel; }
  std::ostringstream out, err;
  const int rc = kopruq::io::runCli({"solve", dir.utf8 + "/model.json", "-o", dir.utf8 + "/results.json"}, out, err);
  EXPECT_EQ(rc, 0) << err.str();
  EXPECT_TRUE(std::filesystem::exists(dir.path / "results.json"));
}

TEST(Utf8Path, ConversionIsExact) {
  const auto p = kopruq::io::utf8Path(kDirUtf8);
  const auto back = p.u8string();
  EXPECT_EQ(std::string(back.begin(), back.end()), kDirUtf8);
}
