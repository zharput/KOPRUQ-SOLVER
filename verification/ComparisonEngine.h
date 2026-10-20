#pragma once
// GUI-5: pure numeric comparison, independent of Qt and of how the two values were obtained.
// Implements the task spec's combined absolute/relative criterion:
//   abs(solver - reference) <= absTol(category) * absoluteToleranceScale + relativeTol * abs(reference)
// A single dimensional tolerance is never reused across displacement/rotation/force/moment (the
// task spec explicitly forbids that): each physical quantity has its own documented default
// absolute-tolerance floor, in its own unit. The approved UI layout has only one "Mutlak
// Tolerans" field (not one per category), so that field is exposed here as
// `absoluteToleranceScale`, a dimensionless multiplier on every category's own floor (1.0 keeps
// the documented defaults; >1 relaxes all of them proportionally) -- never a raw tolerance
// applied identically to meters and kilonewtons.
namespace kopruq::verification {

enum class QuantityCategory { Displacement, Rotation, Force, Moment };

struct ToleranceSettings {
  // Defaults, each in the category's own unit (this project's canonical units: m, rad, kN, kNm).
  // Not derived from any code/standard -- a practical "tight enough to catch a real defect,
  // loose enough to tolerate solver/Excel round-off" engineering judgment call, documented here
  // rather than hidden, and overridable via absoluteToleranceScale/relativeTol from the GUI.
  double displacementAbsTol{1.0e-4}; // m
  double rotationAbsTol{1.0e-5};     // rad
  double forceAbsTol{1.0e-2};        // kN
  double momentAbsTol{1.0e-2};       // kNm
  double relativeTol{1.0e-2};        // dimensionless; legitimately shared across categories
  double absoluteToleranceScale{1.0}; // the GUI's single "Mutlak Tolerans" field (see header note)
};

double categoryAbsoluteTolerance(QuantityCategory category, const ToleranceSettings& tol);

enum class ComparisonStatus { Pass, Fail, NotApplicable, Unsupported };

const char* comparisonStatusName(ComparisonStatus status); // "PASS"/"FAIL"/"N/A"/"UNSUPPORTED"

struct ComparisonResult {
  ComparisonStatus status{ComparisonStatus::NotApplicable};
  double solverValue{0}, referenceValue{0}, signedDiff{0}, absDiff{0}, relDiff{0};
};

// Compares one solver value against one MIDAS reference value for the given physical quantity.
ComparisonResult compareValues(double solverValue, double referenceValue, QuantityCategory category,
                                const ToleranceSettings& tol);

} // namespace kopruq::verification
