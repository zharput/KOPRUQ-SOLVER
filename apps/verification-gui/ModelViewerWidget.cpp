#include "ModelViewerWidget.h"
#include <QColor>
#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QStringList>
#include <QVector4D>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <array>
#include <cmath>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <GL/gl.h>

namespace {
constexpr float kMinDistance = 0.05f;
constexpr int kPickPixelRadius = 8;    // node pick tolerance
constexpr int kPickLinePixelRadius = 6; // beam pick tolerance (distance to segment)
constexpr int kClickMoveTolerance = 4; // press->release movement under this is a "click", not a drag

QString formatRestraints(const std::array<bool, 6>& r) {
  static const char* names[6] = {"UX", "UY", "UZ", "RX", "RY", "RZ"};
  QStringList held;
  for (int i = 0; i < 6; ++i) if (r[i]) held << names[i];
  return held.isEmpty() ? QStringLiteral("(serbest)") : held.join(", ");
}
}

ModelViewerWidget::ModelViewerWidget(QWidget* parent) : QOpenGLWidget(parent) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(false);
}

void ModelViewerWidget::setModel(const kopruq::io::ModelInput& model) {
  model_ = model;
  nodes_.clear();
  beams_.clear();
  selectedNodeId_.reset();
  selectedElementId_.reset();

  nodes_.reserve(model_.model.nodes.size());
  for (const auto& n : model_.model.nodes)
    nodes_.push_back({n.id, QVector3D(static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z))});

  auto findPos = [&](kopruq::Id id) -> const QVector3D* {
    for (const auto& n : nodes_) if (n.id == id) return &n.pos;
    return nullptr;
  };
  beams_.reserve(model_.model.beams.size());
  for (const auto& b : model_.model.beams) {
    const QVector3D* p1 = findPos(b.node1);
    const QVector3D* p2 = findPos(b.node2);
    if (p1 && p2) beams_.push_back({b.id, b.node1, b.node2, b.material, b.section, *p1, *p2, b.reference_y});
  }
  links_.clear();
  links_.reserve(model_.model.springs.size());
  for (const auto& s : model_.model.springs) {
    const QVector3D* p1 = findPos(s.node1);
    const QVector3D* p2 = findPos(s.node2);
    if (p1 && p2) links_.push_back({s.id, *p1, *p2});
  }
  hasModel_ = !nodes_.empty();
  rebuildGeometry();
  fitToModel();
  emit selectionCleared();
  update();
}

void ModelViewerWidget::clearModel() {
  model_ = kopruq::io::ModelInput{};
  nodes_.clear();
  beams_.clear();
  links_.clear();
  hasModel_ = false;
  selectedNodeId_.reset();
  selectedElementId_.reset();
  boundsMin_ = boundsMax_ = QVector3D(0, 0, 0);
  emit selectionCleared();
  update();
}

void ModelViewerWidget::rebuildGeometry() {
  if (nodes_.empty()) { boundsMin_ = boundsMax_ = QVector3D(0, 0, 0); return; }
  boundsMin_ = boundsMax_ = nodes_.front().pos;
  for (const auto& n : nodes_) {
    const QVector3D& p = n.pos;
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
  upVector_ = QVector3D(0, 0, 1);
  update();
}

// View presets, per the approved convention: Üstten (Top) screen-horizontal=+X/vertical=+Y,
// Önden (Front) screen-horizontal=+X/vertical=+Z, Yandan (Side) screen-horizontal=-Y/vertical=+Z.
// Derived from the same screen-right/screen-up projection used for panning (R = cross(forward,
// up), U = cross(R, forward)), not guessed.
void ModelViewerWidget::viewIsometric() { yawDeg_ = -45.0f; pitchDeg_ = 30.0f; upVector_ = QVector3D(0, 0, 1); update(); }
void ModelViewerWidget::viewTop() { yawDeg_ = 0.0f; pitchDeg_ = 90.0f; upVector_ = QVector3D(0, 1, 0); update(); }
void ModelViewerWidget::viewFront() { yawDeg_ = -90.0f; pitchDeg_ = 0.0f; upVector_ = QVector3D(0, 0, 1); update(); }
void ModelViewerWidget::viewSide() { yawDeg_ = 0.0f; pitchDeg_ = 0.0f; upVector_ = QVector3D(0, 0, 1); update(); }
void ModelViewerWidget::zoomIn() { distance_ = std::max(distance_ * 0.8f, kMinDistance); update(); }
void ModelViewerWidget::zoomOut() { distance_ = distance_ * 1.25f; update(); }

void ModelViewerWidget::setShowNodeIds(bool on) { showNodeIds_ = on; update(); }
void ModelViewerWidget::setShowElementIds(bool on) { showElementIds_ = on; update(); }
void ModelViewerWidget::setShowSupports(bool on) { showSupports_ = on; update(); }
void ModelViewerWidget::setShowElasticLinks(bool on) { showElasticLinks_ = on; update(); }
void ModelViewerWidget::setShowLocalAxes(bool on) { showLocalAxes_ = on; update(); }

ModelViewerWidget::SupportKind ModelViewerWidget::classifyRestraint(const std::array<bool, 6>& r) {
  const bool allSix = r[0] && r[1] && r[2] && r[3] && r[4] && r[5];
  if (allSix) return SupportKind::Fixed;
  const bool translationsOnly = r[0] && r[1] && r[2] && !r[3] && !r[4] && !r[5];
  if (translationsOnly) return SupportKind::Pinned;
  const bool verticalOnly = !r[0] && !r[1] && r[2] && !r[3] && !r[4] && !r[5];
  if (verticalOnly) return SupportKind::Roller;
  return SupportKind::Custom;
}

void ModelViewerWidget::initializeGL() {
  glClearColor(0x0B / 255.0f, 0x15 / 255.0f, 0x25 / 255.0f, 1.0f); // Viewer Background #0B1525
  glEnable(GL_DEPTH_TEST);
}

void ModelViewerWidget::resizeGL(int w, int h) {
  viewportW_ = w;
  viewportH_ = h <= 0 ? 1 : h;
  glViewport(0, 0, w, viewportH_);
}

void ModelViewerWidget::applyCameraMatrix() {
  const int w = width(), h = std::max(height(), 1);
  const float aspect = static_cast<float>(w) / static_cast<float>(h);

  QMatrix4x4 proj;
  proj.perspective(45.0f, aspect, 0.01f, std::max(distance_ * 100.0f, 1000.0f));

  const float yaw = qDegreesToRadians(yawDeg_);
  const float pitch = qDegreesToRadians(pitchDeg_);
  // KOPRUQ-SOLVER's global axes are UX/UY/UZ with Z vertical (CLAUDE.md) -- yaw sweeps the
  // horizontal XY plane, pitch lifts the eye along Z, and Z is the up vector for every view.
  const QVector3D eye = target_ + distance_ * QVector3D(-std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));

  QMatrix4x4 view;
  view.lookAt(eye, target_, upVector_);

  lastMvp_ = proj * view;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(lastMvp_.constData());
}

namespace {
// Highlight color for the current selection, #F59E0B.
void setSelectedColor() { glColor3f(0xF5 / 255.0f, 0x9E / 255.0f, 0x0B / 255.0f); }
}

void ModelViewerWidget::paintGL() {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  applyCameraMatrix();
  if (!hasModel_) return;

  // Frame elements: wireframe, #38BDF8 (selected element overridden to #F59E0B).
  glBegin(GL_LINES);
  for (const auto& b : beams_) {
    if (selectedElementId_ && *selectedElementId_ == b.id) setSelectedColor();
    else glColor3f(0x38 / 255.0f, 0xBD / 255.0f, 0xF8 / 255.0f);
    glVertex3f(b.a.x(), b.a.y(), b.a.z());
    glVertex3f(b.b.x(), b.b.y(), b.b.z());
  }
  glEnd();

  // Nodes: #E2E8F0 (selected node overridden to #F59E0B), drawn slightly larger when selected.
  glPointSize(5.0f);
  glBegin(GL_POINTS);
  for (const auto& n : nodes_) {
    if (selectedNodeId_ && *selectedNodeId_ == n.id) continue; // drawn separately, larger, below
    glColor3f(0xE2 / 255.0f, 0xE8 / 255.0f, 0xF0 / 255.0f);
    glVertex3f(n.pos.x(), n.pos.y(), n.pos.z());
  }
  glEnd();
  if (selectedNodeId_) {
    for (const auto& n : nodes_) {
      if (n.id != *selectedNodeId_) continue;
      glPointSize(9.0f);
      setSelectedColor();
      glBegin(GL_POINTS);
      glVertex3f(n.pos.x(), n.pos.y(), n.pos.z());
      glEnd();
      break;
    }
  }

  const QVector3D extent = boundsMax_ - boundsMin_;
  const float symbolSize = std::max(std::max({extent.x(), extent.y(), extent.z()}) * 0.035f, 0.05f);
  if (showElasticLinks_) drawElasticLinks();
  if (showSupports_) drawSupports(symbolSize);
  if (showLocalAxes_) drawLocalAxes(symbolSize);
  drawAxisGizmo();
}

void ModelViewerWidget::drawElasticLinks() {
  // Elastic links: distinctly colored separate lines, #A78BFA.
  glColor3f(0xA7 / 255.0f, 0x78 / 255.0f, 0xFA / 255.0f);
  glBegin(GL_LINES);
  for (const auto& l : links_) {
    glVertex3f(l.a.x(), l.a.y(), l.a.z());
    glVertex3f(l.b.x(), l.b.y(), l.b.z());
  }
  glEnd();
}

void ModelViewerWidget::drawSupports(float s) {
  // Supports: #22C55E. Symbol depends only on the actual restraint pattern -- never a guess for
  // an ambiguous pattern (drawn as a generic diamond instead).
  glColor3f(0x22 / 255.0f, 0xC5 / 255.0f, 0x55 / 255.0f);
  for (const auto& [nodeId, restraint] : model_.restraints) {
    const NodeEntry* node = nullptr;
    for (const auto& n : nodes_) if (n.id == nodeId) { node = &n; break; }
    if (!node) continue;
    const QVector3D& p = node->pos;
    const SupportKind kind = classifyRestraint(restraint);
    switch (kind) {
      // Symbols hang below the node along -Z (KOPRUQ-SOLVER's vertical axis), in the X/Z plane.
      case SupportKind::Fixed: {
        glBegin(GL_LINE_LOOP);
        glVertex3f(p.x()-s, p.y(), p.z()-s); glVertex3f(p.x()+s, p.y(), p.z()-s);
        glVertex3f(p.x()+s, p.y(), p.z()-3*s); glVertex3f(p.x()-s, p.y(), p.z()-3*s);
        glEnd();
        glBegin(GL_LINES);
        glVertex3f(p.x(), p.y(), p.z()); glVertex3f(p.x(), p.y(), p.z()-s);
        glEnd();
        break;
      }
      case SupportKind::Pinned: {
        glBegin(GL_LINE_LOOP);
        glVertex3f(p.x(), p.y(), p.z());
        glVertex3f(p.x()-s, p.y(), p.z()-2*s);
        glVertex3f(p.x()+s, p.y(), p.z()-2*s);
        glEnd();
        break;
      }
      case SupportKind::Roller: {
        constexpr int kSegments = 16;
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < kSegments; ++i) {
          const float a = 2.0f * 3.14159265f * i / kSegments;
          glVertex3f(p.x() + s * std::cos(a), p.y(), p.z() - 2*s + s * std::sin(a));
        }
        glEnd();
        break;
      }
      case SupportKind::Custom: {
        glBegin(GL_LINE_LOOP);
        glVertex3f(p.x(), p.y(), p.z()-2*s);
        glVertex3f(p.x()-s, p.y(), p.z()-s);
        glVertex3f(p.x(), p.y(), p.z());
        glVertex3f(p.x()+s, p.y(), p.z()-s);
        glEnd();
        break;
      }
    }
  }
}

void ModelViewerWidget::drawLocalAxes(float s) {
  // Local axis arrows: computed by calling the solver's own kopruq::Beam3D::axes() for the
  // exact same geometry it uses (node positions + reference_y), never a parallel visual-only
  // calculation. Red/green/blue = local x/y/z, drawn from each beam's midpoint.
  for (const auto& b : beams_) {
    kopruq::Beam3DGeometry g{Eigen::Vector3d(b.a.x(), b.a.y(), b.a.z()), Eigen::Vector3d(b.b.x(), b.b.y(), b.b.z()), b.referenceY};
    kopruq::Beam3DAxes axes;
    try { axes = kopruq::Beam3D::axes(g); } catch (const std::exception&) { continue; } // never fabricate on a degenerate beam
    const QVector3D mid = (b.a + b.b) * 0.5f;
    auto drawArrow = [&](const Eigen::Vector3d& dir, float r, float gr, float bl) {
      const QVector3D d(static_cast<float>(dir.x()), static_cast<float>(dir.y()), static_cast<float>(dir.z()));
      glColor3f(r, gr, bl);
      glBegin(GL_LINES);
      glVertex3f(mid.x(), mid.y(), mid.z());
      glVertex3f(mid.x() + d.x()*s, mid.y() + d.y()*s, mid.z() + d.z()*s);
      glEnd();
    };
    drawArrow(axes.x, 1.0f, 0.2f, 0.2f);
    drawArrow(axes.y, 0.2f, 1.0f, 0.2f);
    drawArrow(axes.z, 0.3f, 0.5f, 1.0f);
  }
}

void ModelViewerWidget::drawAxisGizmo() {
  // Corner XYZ gizmo: rotation-only camera, origin-centered, small fixed-size viewport region
  // in the bottom-left, color-coded red/green/blue, updates as the camera orbits.
  const int size = std::min({80, width(), height()});
  if (size <= 0) return;
  glViewport(0, 0, size, size);
  glClear(GL_DEPTH_BUFFER_BIT);

  QMatrix4x4 proj;
  proj.ortho(-1.6f, 1.6f, -1.6f, 1.6f, -10.0f, 10.0f);
  const float yaw = qDegreesToRadians(yawDeg_);
  const float pitch = qDegreesToRadians(pitchDeg_);
  const QVector3D dir(-std::cos(pitch) * std::cos(yaw), std::cos(pitch) * std::sin(yaw), std::sin(pitch));
  QMatrix4x4 view;
  view.lookAt(dir * 4.0f, QVector3D(0, 0, 0), upVector_);
  QMatrix4x4 mvp = proj * view;
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glMatrixMode(GL_MODELVIEW);
  glLoadMatrixf(mvp.constData());

  glBegin(GL_LINES);
  glColor3f(1, 0, 0); glVertex3f(0, 0, 0); glVertex3f(1, 0, 0); // X: red
  glColor3f(0, 1, 0); glVertex3f(0, 0, 0); glVertex3f(0, 1, 0); // Y: green
  glColor3f(0.3f, 0.5f, 1.0f); glVertex3f(0, 0, 0); glVertex3f(0, 0, 1); // Z: blue
  glEnd();

  glViewport(0, 0, viewportW_, viewportH_);
}

QPointF ModelViewerWidget::projectToScreen(const QVector3D& worldPos, bool* visible) const {
  const QVector4D clip = lastMvp_ * QVector4D(worldPos, 1.0f);
  if (visible) *visible = clip.w() > 1e-6f;
  if (clip.w() <= 1e-6f) return QPointF(-1e6, -1e6);
  const float ndcX = clip.x() / clip.w();
  const float ndcY = clip.y() / clip.w();
  // devicePixelRatio: QOpenGLWidget's GL viewport is in device pixels, but QPainter overlay
  // drawing (and mouse events) use logical (widget) pixels, so project into logical pixels here.
  const float wpx = static_cast<float>(width());
  const float hpx = static_cast<float>(height());
  return QPointF((ndcX * 0.5f + 0.5f) * wpx, (1.0f - (ndcY * 0.5f + 0.5f)) * hpx);
}

QString ModelViewerWidget::describeNode(const NodeEntry& n) const {
  QString text = QStringLiteral("Düğüm Bilgisi\n\nID: %1\nX: %2 m\nY: %3 m\nZ: %4 m\n")
    .arg(n.id).arg(n.pos.x(), 0, 'f', 4).arg(n.pos.y(), 0, 'f', 4).arg(n.pos.z(), 0, 'f', 4);
  auto it = model_.restraints.find(n.id);
  text += QStringLiteral("Mesnet Koşulları: %1")
    .arg(it != model_.restraints.end() ? formatRestraints(it->second) : QStringLiteral("(serbest)"));
  return text;
}

QString ModelViewerWidget::describeBeam(const BeamEntry& b) const {
  const double length = (b.b - b.a).length();
  return QStringLiteral("Eleman Bilgisi\n\nID: %1\nTip: Beam3D\nBaşlangıç Düğüm: %2\nBitiş Düğüm: %3\n"
                         "Uzunluk: %4 m\nKesit (Section) ID: %5\nMalzeme (Material) ID: %6")
    .arg(b.id).arg(b.node1).arg(b.node2).arg(length, 0, 'f', 4).arg(b.section).arg(b.material);
}

void ModelViewerWidget::drawOverlayLabels(QPainter& painter) {
  if (!hasModel_ || (!showNodeIds_ && !showElementIds_)) return;
  painter.setPen(QColor(0xE5, 0xEA, 0xF2)); // Text #E5EAF2
  if (showNodeIds_) {
    for (const auto& n : nodes_) {
      bool visible = false;
      const QPointF s = projectToScreen(n.pos, &visible);
      if (visible) painter.drawText(s + QPointF(6, -6), QString::number(n.id));
    }
  }
  if (showElementIds_) {
    for (const auto& b : beams_) {
      bool v1 = false, v2 = false;
      const QPointF s1 = projectToScreen(b.a, &v1);
      const QPointF s2 = projectToScreen(b.b, &v2);
      if (v1 && v2) painter.drawText((s1 + s2) * 0.5, QString::number(b.id));
    }
  }
}

void ModelViewerWidget::paintEvent(QPaintEvent* event) {
  QOpenGLWidget::paintEvent(event); // makeCurrent + paintGL + doneCurrent, composited into this widget
  QPainter painter(this);
  drawOverlayLabels(painter);
}

void ModelViewerWidget::pickAt(const QPoint& pos) {
  if (!hasModel_) { emit selectionCleared(); return; }

  kopruq::Id bestNode = 0;
  double bestNodeDist = kPickPixelRadius;
  for (const auto& n : nodes_) {
    bool visible = false;
    const QPointF s = projectToScreen(n.pos, &visible);
    if (!visible) continue;
    const double d = QLineF(s, pos).length();
    if (d < bestNodeDist) { bestNodeDist = d; bestNode = n.id; }
  }
  if (bestNode != 0) {
    selectedNodeId_ = bestNode;
    selectedElementId_.reset();
    for (const auto& n : nodes_) if (n.id == bestNode) { emit nodeInfoChanged(describeNode(n)); break; }
    update();
    return;
  }

  kopruq::Id bestBeam = 0;
  double bestBeamDist = kPickLinePixelRadius;
  for (const auto& b : beams_) {
    bool v1 = false, v2 = false;
    const QPointF s1 = projectToScreen(b.a, &v1);
    const QPointF s2 = projectToScreen(b.b, &v2);
    if (!v1 || !v2) continue;
    const QLineF seg(s1, s2);
    const double len = seg.length();
    double d;
    if (len < 1e-6) {
      d = QLineF(s1, pos).length();
    } else {
      const double t = std::clamp(QPointF::dotProduct(pos - s1, s2 - s1) / (len * len), 0.0, 1.0);
      const QPointF proj = s1 + t * (s2 - s1);
      d = QLineF(proj, pos).length();
    }
    if (d < bestBeamDist) { bestBeamDist = d; bestBeam = b.id; }
  }
  if (bestBeam != 0) {
    selectedElementId_ = bestBeam;
    selectedNodeId_.reset();
    for (const auto& b : beams_) if (b.id == bestBeam) { emit elementInfoChanged(describeBeam(b)); break; }
    update();
    return;
  }

  selectedNodeId_.reset();
  selectedElementId_.reset();
  emit selectionCleared();
  update();
}

void ModelViewerWidget::mousePressEvent(QMouseEvent* event) {
  lastMousePos_ = pressPos_ = event->pos();
  rotating_ = (event->button() == Qt::LeftButton);
  panning_ = (event->button() == Qt::MiddleButton);
  // Leaving the Top preset's special up=(0,1,0) requires reverting to the standard Z-up vector
  // before any further orbiting, since (0,1,0) is only valid for that one exact view.
  if (rotating_) upVector_ = QVector3D(0, 0, 1);
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
    const QVector3D right(-std::sin(yaw), -std::cos(yaw), 0.0f);
    const QVector3D up(0.0f, 0.0f, 1.0f); // Z vertical
    const float panScale = distance_ * 0.002f;
    target_ -= right * (delta.x() * panScale);
    target_ += up * (delta.y() * panScale);
    update();
  }
}

void ModelViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
  const bool wasRotating = rotating_;
  rotating_ = false;
  panning_ = false;
  if (event->button() == Qt::LeftButton && wasRotating &&
      (event->pos() - pressPos_).manhattanLength() <= kClickMoveTolerance) {
    pickAt(event->pos());
  }
}

void ModelViewerWidget::wheelEvent(QWheelEvent* event) {
  const float factor = event->angleDelta().y() > 0 ? 0.9f : 1.1f;
  distance_ = std::max(distance_ * factor, kMinDistance);
  update();
}
