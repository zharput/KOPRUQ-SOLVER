#include <iostream>
#include <string>
#include <vector>
#include "kopruq/io/Cli.hpp"
// Command-line arguments are handed to runCli as UTF-8 (paths are opened via io::utf8Path). On
// Windows the narrow argv is in the ANSI code page and cannot represent every character of a path
// (e.g. Turkish s/g/i with diacritics under code page 1252), so the wide argv is converted to UTF-8.
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
static std::string toUtf8(const wchar_t* w) {
  const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) return {};
  std::string s(static_cast<std::size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
  return s;
}
int wmain(int argc, wchar_t** argv) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) args.push_back(toUtf8(argv[i]));
  return kopruq::io::runCli(args, std::cout, std::cerr);
}
#else
int main(int argc, char** argv) { std::vector<std::string> args(argv + 1, argv + argc); return kopruq::io::runCli(args, std::cout, std::cerr); }
#endif
