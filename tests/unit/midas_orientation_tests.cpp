// Phase 7B — MIDAS beta-angle/local-axis -> reference_y conversion
// (MidasOrientation.hpp). Every expected value is derived directly from
// the rule quoted from MIDAS's own manual (see that file's header
// comment): horizontal/inclined members default to local z = projection
// of global Z; vertical members (local x parallel to global Z) default
// to local z = global X; Beta then rotates (y,z) about local x.
//
// Checks feed midasReferenceY()'s output into Beam3D::axes() (an
// independently verified primitive, Phase 3.2R1) and confirm the
// resulting local z axis matches the hand-derived expectation directly,
// not by re-deriving midasReferenceY's own formula.
#include <gtest/gtest.h>
#include <Eigen/Dense>
#include <cmath>
#include "kopruq/io/midas/MidasOrientation.hpp"
#include "kopruq/elements/Beam3D.hpp"

using namespace kopruq::io::midas;
using kopruq::Beam3D;
using kopruq::Beam3DGeometry;

namespace {
void vrel(const Eigen::Vector3d& actual, const Eigen::Vector3d& expected, const char* what) {
  EXPECT_NEAR((actual - expected).norm(), 0.0, 1e-9) << what << ": got (" << actual.transpose() << ") expected (" << expected.transpose() << ")";
}
}

TEST(MidasOrientation, HorizontalMemberBetaZeroGivesLocalZAlongGlobalZ) {
  Eigen::Vector3d x(1, 0, 0);
  auto refY = midasReferenceY(x, 0.0);
  auto axes = Beam3D::axes(Beam3DGeometry{Eigen::Vector3d(0,0,0), x, refY});
  vrel(axes.z, Eigen::Vector3d(0,0,1), "local z (beta=0, horizontal member)");
}

TEST(MidasOrientation, VerticalMemberBetaZeroGivesLocalZAlongGlobalX) {
  Eigen::Vector3d x(0, 0, 1);
  auto refY = midasReferenceY(x, 0.0);
  auto axes = Beam3D::axes(Beam3DGeometry{Eigen::Vector3d(0,0,0), x, refY});
  vrel(axes.z, Eigen::Vector3d(1,0,0), "local z (beta=0, vertical member)");
}

TEST(MidasOrientation, HorizontalMemberBeta90RotatesZToPreBetaY) {
  Eigen::Vector3d x(1, 0, 0);
  auto refY0 = midasReferenceY(x, 0.0);
  auto axes0 = Beam3D::axes(Beam3DGeometry{Eigen::Vector3d(0,0,0), x, refY0});
  auto refY90 = midasReferenceY(x, 90.0);
  auto axes90 = Beam3D::axes(Beam3DGeometry{Eigen::Vector3d(0,0,0), x, refY90});
  vrel(axes90.z, axes0.y, "local z at beta=90 equals local y at beta=0");
}

TEST(MidasOrientation, InclinedMemberBetaZeroZIsPerpendicularProjectionOfGlobalZ) {
  Eigen::Vector3d x(3, 4, 0); x.normalize(); // inclined in the horizontal plane
  auto refY = midasReferenceY(x, 0.0);
  auto axes = Beam3D::axes(Beam3DGeometry{Eigen::Vector3d(0,0,0), x, refY});
  // Hand check: global Z has no component along this (horizontal) x, so the
  // projection is global Z itself, same as the purely-horizontal case.
  vrel(axes.z, Eigen::Vector3d(0,0,1), "local z for a horizontal-plane inclined member");
  EXPECT_NEAR(axes.z.dot(x), 0.0, 1e-9);
  EXPECT_NEAR(axes.y.dot(x), 0.0, 1e-9);
}

TEST(MidasOrientation, RejectsNonFiniteInput) {
  EXPECT_THROW(midasReferenceY(Eigen::Vector3d(1,0,0), std::nan("")), std::invalid_argument);
  EXPECT_THROW(midasReferenceY(Eigen::Vector3d(std::nan(""),0,0), 0.0), std::invalid_argument);
}
