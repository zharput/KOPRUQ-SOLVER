# KOPRUQ-SOLVER

Independent C++20 foundation for a future finite-element bridge solver. Phase 0 provides domain types, a public API shell, CLI version/help commands, Eigen integration, and test scaffolding. Numerical analysis is intentionally not implemented yet.

## Build

The project uses the Visual Studio Build Tools bundled CMake, Ninja, MSVC x64 environment, and standalone vcpkg at `C:\_ZHarput_Data\Tools\vcpkg`. From a normal PowerShell terminal, run:

```powershell
./scripts/configure.ps1
./scripts/build.ps1
./scripts/test.ps1
```

The scripts set `VCPKG_ROOT` for the current process only and do not modify the global PATH. Release builds use the `ninja-release` configure and build presets. The vcpkg registry is pinned by the `builtin-baseline` in `vcpkg.json`.

The installed Visual Studio vcpkg may require PowerShell Core during compiler detection. Install it separately only if vcpkg reports that requirement; this project does not install tools automatically.
