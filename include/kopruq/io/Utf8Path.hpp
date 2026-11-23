#pragma once
// File paths travel through KOPRUQ-SOLVER as UTF-8 std::string (2026-11-23). On Windows a narrow
// std::ifstream/ofstream interprets its path in the ANSI code page (e.g. 1252), so a UTF-8 path with
// non-ASCII characters (verification model 100, folder "100 Basit köprü") silently fails to open.
// utf8Path converts to std::filesystem::path through char8_t, which is exact on every platform;
// open files only through it.
#include <filesystem>
#include <string>

namespace kopruq::io {

inline std::filesystem::path utf8Path(const std::string& utf8) {
  return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace kopruq::io
