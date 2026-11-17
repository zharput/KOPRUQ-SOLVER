#pragma once
// V2.2 Stage A (Viewer Foundation): a read-only 3D wireframe viewer for the exact MIDAS model
// the solver analyzes. Geometry comes solely from kopruq::io::ModelInput (populated by
// kopruq::io::readModelFile(..., "midas-nx-json", ...), the same importer path the CLI/solver
// uses) -- never from a separate parser. Stage B (ID labels, selection, info panel) and Stage C
// (supports, elastic links, local axes, display toggles) are later, separately-approved stages;
// this widget intentionally only draws nodes and beam wireframes plus basic camera controls.
#include "../../include/kopruq/io/ModelJson.hpp"
#include <QOpenGLWidget>
#include <QPoint>
#include <QVector3D>
#include <vector>

// Stage A deliberately uses the desktop fixed-function pipeline (glBegin/glColor3f/...) rather
// than a shader-based core-profile renderer: a few thousand lines/points need no vertex buffers
// or shaders to stay responsive, and it keeps this stage's scope to "basic node/beam drawing"
// as specified, without introducing GLSL build/version complexity this project doesn't need yet.
class ModelViewerWidget : public QOpenGLWidget {
  Q_OBJECT
public:
  explicit ModelViewerWidget(QWidget* parent = nullptr);

  // Replaces the displayed model. Pass an empty optional (or call clearModel()) to show nothing
  // -- used when the MIDAS file selection changes but has not (yet) passed import/validation.
  void setModel(const kopruq::io::ModelInput& model);
  void clearModel();

public slots:
  void fitToModel();
  void resetView();
  void viewIsometric();
  void viewTop();
  void viewFront();
  void viewSide();
  void zoomIn();
  void zoomOut();

protected:
  void initializeGL() override;
  void resizeGL(int w, int h) override;
  void paintGL() override;
  void mousePressEvent(QMouseEvent* event) override;
  void mouseMoveEvent(QMouseEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

private:
  struct Beam { QVector3D a, b; };

  void rebuildGeometry();
  void applyCameraMatrix();

  std::vector<QVector3D> nodePositions_;
  std::vector<Beam> beams_;
  QVector3D boundsMin_{0, 0, 0}, boundsMax_{0, 0, 0};
  bool hasModel_{false};

  // Camera: orbits a target point at a given distance; yaw/pitch in degrees.
  QVector3D target_{0, 0, 0};
  float distance_{10.0f};
  float yawDeg_{-45.0f};
  float pitchDeg_{25.0f};
  QPoint lastMousePos_;
  bool rotating_{false};
  bool panning_{false};
};
