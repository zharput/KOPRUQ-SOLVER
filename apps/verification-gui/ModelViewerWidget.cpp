#include "ModelViewerWidget.h"
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>

namespace {
constexpr float kMinDistance = 0.05f;
}

ModelViewerWidget::ModelViewerWidget(QWidget* parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
}

void ModelViewerWidget::setModel(const kopruq::io::ModelInput& model) {
  nodePositions_.clear();
  beams_.clear();
  std::vector<QVector3D> byIndex; // positions keyed by node id lookup below
  std::vector<std::pair<kopruq::Id, QVector3D>> byId;
  byId.reserve(model.model.nodes.size());
  for (const auto& n : model.model.nodes) {
    QVector3D p(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z));
    nodePositions_.push_back(p);
    byId.emplace_back(n.id, p);
  }
  auto findPos = [&](kopruq::Id id) -> const QVector3D* {
    for (const auto& [nid, p] : byId) if (nid == id) return &p;
    return nullptr;
  };
  beams_.reserve(model.model.beams.size());
  for (const auto& b : model.model.beams) {
    const QVector3D* p1 = findPos(b.node1);
    const QVector3D* p2 = findPos(b.node2);
    if (p1 && p2) beams_.push_back({*p1, *p2});
  }
  hasModel_ = !nodePositions_.empty();
  rebuildGeometry();
  fitToModel();
  update();
}

void ModelViewerWidget::clearModel() {
  nodePositions_.clear();
  beams_.clear();
  hasModel_ = false;
  boundsMin_ = boundsMax_ = QVector3D(0, 0, 0);
  update();
}

void ModelViewerWidget::rebuildGeometry() {
  if (nodePositions_.empty()) { boundsMin_ = boundsMax_ = QVector3D(0, 0, 0); return; }
  boundsMin_ = boundsMax_ = nodePositions_.front();
  for (const auto& p : nodePositions_) {
    boundsMin_.setX(std::min(boundsMin_.x(), p.x())); boundsMax_.setX(std::max(boundsMax_.x(), p.x()));
    boundsMin_.setY(std::min(boundsMin_.y(), p.y())); boundsMax_.setY(std::max(boundsMax_.y(), p.y()));
    boundsMin_.setZ(std::min(boundsMin_.z(), p.z())); boundsMax_.setZ(std::max(boundsMax_.z(), p.z()));
  }
}

void ModelViewerWidget::fitToModel() {
  if (!hasModel_) { resetView(); return; }
  target_ = (boundsMin_ + boundsMax_) * 0.5f;
  const QVector3D extent = boundsMax_ - boundsMin_;
  float radius = std::max({extent.x(), extent.y(), extent.z(), 1.0f}) * 0.5f;
  distance_ = std::max(radius * 2.5f, kMinDistance);
  update();
}

void ModelViewerWidget::resetView() {
  target_ = QVector3D(0, 0, 0);
  distance_ = 10.0f;
  yawDeg_ = -45.0f;
  pitchDeg_ = 25.0f;
  update();
}

void ModelViewerWidget::viewIsometric() { yawDeg_ = -45.0f; pitchDeg_ = 30.0f; update(); }
void ModelViewerWidget::viewTop() { yawDeg_ = 0.0f; pitchDeg_ = 89.9f; update(); }
void ModelViewerWidget::viewFront() { yawDeg_ = 0.0f; pitchDeg_ = 0.0f; update(); }
void ModelViewerWidget::viewSide() { yawDeg_ = 90.0f; pitchDeg_ = 0.0f; update(); }
void ModelViewerWidget::zoomIn() { distance_ = std::max(distance_ * 0.8f, kMinDistance); update(); }
void ModelViewerWidget::zoomOut() { distance_ = distance_ * 1.25f; update(); }

void ModelViewerWidget::initializeGL() {
  glClearColor(0x0B / 255.0f, 0x15 / 255.0f, 0x25 / 255.0f, 1.0f); // Viewer Background #0B1525
  glEnable(GL_DEPTH_TEST);
}

void ModelViewerWidget::resizeGL(int w, int h) {
  glViewport(0, 0, w, h <= 0 ? 1 : h);
}

void ModelViewerWidget::applyCameraMatrix() {
  const int w = width(), h = std::max(height(), 1);
  const float aspect = static_cast<float>(w) / static_cast<float>(h);

  QMatrix4x4 proj;
  proj.perspective(45.0f, aspect, 0.01f, std::max(distance_ * 100.0f, 1000.0f));

  const float yaw = qDegreesToRadians(yawDeg_);
  const float pitch = qDegreesToRadians(pitchDeg_);
  const QVector3D eye = target_ + distance_ * QVector3D(std::cos(pitch) * std::cos(yaw), std::sin(pitch), std::cos(pitch) * std::sin(yaw));

  QMatrix4x4 view;
  view.lookAt(eye, target_, QVector3D(0, 1, 0)); // Y is the consistent vertical axis for every view

  QMatrix4x4 mvp = proj * view;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(mvp.constData());
}

void ModelViewerWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  applyCameraMatrix();
  if (!hasModel_) return;

  // Frame elements: wireframe, #38BDF8.
  glColor3f(0x38 / 255.0f, 0xBD / 255.0f, 0xF8 / 255.0f);
  glBegin(GL_LINES);
  for (const auto& b : beams_) {
    glVertex3f(b.a.x(), b.a.y(), b.a.z());
    glVertex3f(b.b.x(), b.b.y(), b.b.z());
  }
  glEnd();

  // Nodes: #E2E8F0.
  glColor3f(0xE2 / 255.0f, 0xE8 / 255.0f, 0xF0 / 255.0f);
  glPointSize(5.0f);
  glBegin(GL_POINTS);
  for (const auto& p : nodePositions_) glVertex3f(p.x(), p.y(), p.z());
  glEnd();
}

void ModelViewerWidget::mousePressEvent(QMouseEvent* event) {
  lastMousePos_ = event->pos();
  rotating_ = (event->button() == Qt::LeftButton);
  panning_ = (event->button() == Qt::MiddleButton);
}

void ModelViewerWidget::mouseMoveEvent(QMouseEvent* event) {
  const QPoint delta = event->pos() - lastMousePos_;
  lastMousePos_ = event->pos();
  if (rotating_) {
    yawDeg_ += delta.x() * 0.4f;
    pitchDeg_ = std::clamp(pitchDeg_ + delta.y() * 0.4f, -89.0f, 89.0f);
    update();
  } else if (panning_) {
    const float yaw = qDegreesToRadians(yawDeg_);
    const QVector3D right(-std::sin(yaw), 0.0f, std::cos(yaw));
    const QVector3D up(0.0f, 1.0f, 0.0f);
    const float panScale = distance_ * 0.002f;
    target_ -= right * (delta.x() * panScale);
    target_ += up * (delta.y() * panScale);
    update();
  }
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event) {
  const float factor = event->angleDelta().y() > 0 ? 0.9f : 1.1f;
  distance_ = std::max(distance_ * factor, kMinDistance);
  update();
}
