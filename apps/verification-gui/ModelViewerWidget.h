#pragma once
// V2.2 Stage A (Viewer Foundation) + Stage B (Model Interaction) + Stage C (Structural Display):
// a read-only 3D wireframe viewer for the exact MIDAS model the solver analyzes. Geometry comes
// solely from kopruq::io::ModelInput (populated by kopruq::io::detail::readModelFile(...,
// "midas-nx-json", ...), the same importer path the CLI/solver uses) -- never from a separate
// parser.
//
// Stage B added: optional node/element ID labels (off by default, per the performance
// requirement), click-to-select a node or beam, and an Element/Node Information text report
// (nodeInfoChanged/elementInfoChanged/selectionCleared signals) built only from real model
// fields -- never fabricated.
//
// Stage C adds: support symbols derived only from the actual restraint pattern in
// kopruq::io::ModelInput::restraints (Fixed = all 6 DOFs held, Pinned = all 3 translations held
// and no rotation, Roller = only UZ held; any other pattern draws a distinct, deliberately
// generic "custom restraint" marker rather than guessing a canonical type); elastic links
// (kopruq::Model::springs) drawn as separate #A78BFA lines; local axis arrows computed by
// calling kopruq::Beam3D::axes() directly -- the solver's own orientation/transformation logic,
// never a separate visual-only calculation; and a corner XYZ axis gizmo that rotates with the
// camera. Supports/elastic links default ON (cheap to draw, structurally important); local axes
// and ID labels default OFF (visual clutter / performance).
#include "../../include/kopruq/io/ModelJson.hpp"
#include "../../include/kopruq/elements/Beam3D.hpp"
#include <QMatrix4x4>
#include <QOpenGLWidget>
#include <QPoint>
#include <QString>
#include <QVector3D>
#include <array>
#include <optional>
#include <vector>

// Stage A/B deliberately use the desktop fixed-function pipeline (glBegin/glColor3f/...) rather
// than a shader-based core-profile renderer: a few thousand lines/points need no vertex buffers
// or shaders to stay responsive, and it keeps scope to "basic node/beam drawing" plus simple
// screen-space picking, without introducing GLSL build/version complexity this project doesn't
// need yet.
class ModelViewerWidget : public QOpenGLWidget {
  Q_OBJECT
public:
  explicit ModelViewerWidget(QWidget* parent = nullptr);

  // Replaces the displayed model. Call clearModel() to show nothing -- used when the MIDAS file
  // selection changes but has not (yet) passed import/validation.
  void setModel(const kopruq::io::ModelInput& model);
  void clearModel();

signals:
  void nodeInfoChanged(const QString& text);
  void elementInfoChanged(const QString& text);
  void selectionCleared();

public slots:
  void fitToModel();
  void resetView();
  void viewIsometric();
  void viewTop();
  void viewFront();
  void viewSide();
  void zoomIn();
  void zoomOut();
  void setShowNodeIds(bool on);
  void setShowElementIds(bool on);
  void setShowSupports(bool on);
  void setShowElasticLinks(bool on);
  void setShowLocalAxes(bool on);

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  struct NodeEntry { kopruq::Id id; QVector3D pos; };
  struct BeamEntry { kopruq::Id id, node1, node2, material, section; QVector3D a, b; Eigen::Vector3d referenceY; };
  struct LinkEntry { kopruq::Id id; QVector3D a, b; };
  enum class SupportKind { Fixed, Pinned, Roller, Custom };

  void rebuildGeometry();
  void applyCameraMatrix();
  void drawOverlayLabels(class QPainter& painter);
  void drawSupports(float symbolSize);
  void drawElasticLinks();
  void drawLocalAxes(float symbolSize);
  void drawAxisGizmo();
  QPointF projectToScreen(const QVector3D& worldPos, bool* visible) const;
  void pickAt(const QPoint& pos);
  QString describeNode(const NodeEntry& n) const;
  QString describeBeam(const BeamEntry& b) const;
  static SupportKind classifyRestraint(const std::array<bool, 6>& r);

  kopruq::io::ModelInput model_;
  std::vector<NodeEntry> nodes_;
  std::vector<BeamEntry> beams_;
  std::vector<LinkEntry> links_;
  QVector3D boundsMin_{0, 0, 0}, boundsMax_{0, 0, 0};
  bool hasModel_{false};

  bool showNodeIds_{false};
  bool showElementIds_{false};
  bool showSupports_{true};
  bool showElasticLinks_{true};
  bool showLocalAxes_{false};
  std::optional<kopruq::Id> selectedNodeId_;
  std::optional<kopruq::Id> selectedElementId_;

  QMatrix4x4 lastMvp_;
  int viewportW_{1}, viewportH_{1};

  // Camera: orbits a target point at a given distance; yaw/pitch in degrees. upVector_ is
  // (0,0,1) for every orbiting view, except the Top preset: looking straight down the Z axis
  // makes (0,0,1) parallel to the view direction (an undefined/degenerate "up" for lookAt), so
  // Top uses (0,1,0) instead (screen-vertical = +Y, per the approved view convention). Any
  // manual rotation drag resets it back to (0,0,1).
  QVector3D target_{0, 0, 0};
  QVector3D upVector_{0, 0, 1};
  float distance_{10.0f};
  float yawDeg_{-45.0f};
  float pitchDeg_{25.0f};
  QPoint lastMousePos_;
  QPoint pressPos_;
  bool rotating_{false};
  bool panning_{false};
};
