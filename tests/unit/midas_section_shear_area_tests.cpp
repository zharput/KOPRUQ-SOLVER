// Phase 7B requalification (2026-11-16) — Timoshenko shear-area formulas wired into
// MidasSectionLibrary.hpp (AsH/AsB), approved after the verification/models/02-04 cantilever
// and simply-supported beam cross-checks (see docs/DEVELOPMENT_STATUS.md). Every expected
// value below is hand-computed independently from the classical shear correction factors
// (not by calling the production functions).
#include <gtest/gtest.h>
#include <cmath>
#include "kopruq/io/midas/MidasSectionLibrary.hpp"

using namespace kopruq::io::midas;

namespace {
void rel(double actual, double expected, const char* what, double tol = 1e-9) {
  EXPECT_NEAR(actual, expected, tol * std::max(std::abs(expected), 1e-12)) << what;
}
}

TEST(MidasSectionShearArea, SolidRectangleUsesFiveSixthsOfAreaBothDirections) {
  // H=0.3, B=1.0 (verification/models/01-04's section). k=5/6, isotropic in H/B.
  auto p = solidRectangle(0.3, 1.0);
  const double area = 0.3 * 1.0;
  rel(p.AsH, (5.0/6.0) * area, "AsH = 5/6 * A");
  rel(p.AsB, (5.0/6.0) * area, "AsB = 5/6 * A");
  rel(p.AsH, p.AsB, "rectangle shear factor is isotropic in H/B", 1e-9);
}

TEST(MidasSectionShearArea, SolidCircleUsesPointNineOfArea) {
  auto p = solidCircle(1.2);
  const double area = kPi * (1.2/2.0) * (1.2/2.0);
  rel(p.AsH, 0.9 * area, "AsH = 0.9 * A");
  rel(p.AsB, 0.9 * area, "AsB = 0.9 * A");
}

TEST(MidasSectionShearArea, HollowBoxUsesRigorousZhuravskiiQuadrature) {
  // H=2,B=5,tw=tf=0.5 (same dimensions as the existing HollowBoxMatchesHandComputation_Pier2
  // test in midas_section_library_tests.cpp). 2026-11-19: replaced the earlier coarser "shear
  // carried by the web/flange alone" approximation (AsH=2*tw*(H-tf), AsB=2*tf*(B-tw)) with the
  // rigorous Zhuravskii/Timoshenko quadrature (detail::timoshenkoShearArea) -- see that file's
  // own comment for the derivation. Reference values are an independent computation
  // (scipy.integrate.quad, a separate implementation/language/method), not read back from the
  // production function, matching midas_section_library_tests.cpp's own verification of the
  // same case.
  auto p = hollowBox(2.0, 5.0, 0.5, 0.5);
  rel(p.AsH, 2.188448530780822, "AsH (Zhuravskii quadrature, independent scipy.integrate.quad)", 1e-5);
  rel(p.AsB, 4.4760067199079066, "AsB (Zhuravskii quadrature, independent scipy.integrate.quad)", 1e-5);
}

TEST(MidasSectionShearArea, ShearAreasAreZeroOnlyWhenNotComputed_AlwaysPositiveHere) {
  // All three resolver functions always populate AsH/AsB (unlike Asy/Asz in the imported
  // Section, which the importer zeroes out when USE_SHEAR_DEFORM is false) -- so a direct
  // call always returns a strictly positive shear area for a valid section.
  EXPECT_GT(solidRectangle(0.3, 1.0).AsH, 0.0);
  EXPECT_GT(solidCircle(1.2).AsH, 0.0);
  EXPECT_GT(hollowBox(2.0, 5.0, 0.5, 0.5).AsH, 0.0);
}
