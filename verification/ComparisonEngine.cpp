#include "ComparisonEngine.h"
#include <cmath>

namespace kopruq::verification {

double categoryAbsoluteTolerance(QuantityCategory category, const ToleranceSettings& tol) {
  double base = 0.0;
  switch (category) {
    case QuantityCategory::Displacement: base = tol.displacementAbsTol; break;
    case QuantityCategory::Rotation: base = tol.rotationAbsTol; break;
    case QuantityCategory::Force: base = tol.forceAbsTol; break;
    case QuantityCategory::Moment: base = tol.momentAbsTol; break;
  }
  return base * tol.absoluteToleranceScale;
}

const char* comparisonStatusName(ComparisonStatus status) {
  switch (status) {
    case ComparisonStatus::Pass: return "PASS";
    case ComparisonStatus::Fail: return "FAIL";
    case ComparisonStatus::NotApplicable: return "N/A";
    case ComparisonStatus::Unsupported: return "UNSUPPORTED";
  }
  return "?";
}

ComparisonResult compareValues(double solverValue, double referenceValue, QuantityCategory category,
                                const ToleranceSettings& tol) {
  ComparisonResult r;
  r.solverValue = solverValue;
  r.referenceValue = referenceValue;
  r.signedDiff = solverValue - referenceValue;
  r.absDiff = std::fabs(r.signedDiff);
  r.relDiff = std::fabs(referenceValue) > 0.0 ? r.absDiff / std::fabs(referenceValue) : 0.0;

  if (!std::isfinite(solverValue) || !std::isfinite(referenceValue)) {
    r.status = ComparisonStatus::NotApplicable;
    return r;
  }

  const double floor = categoryAbsoluteTolerance(category, tol) + tol.relativeTol * std::fabs(referenceValue);
  r.status = (r.absDiff <= floor) ? ComparisonStatus::Pass : ComparisonStatus::Fail;
  return r;
}

} // namespace kopruq::verification
