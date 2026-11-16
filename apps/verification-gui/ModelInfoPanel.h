#pragma once
// V2.0: "Model Bilgileri (JSON)" panel -- shows real numbers read back from the MIDAS importer's
// own report (MainWindow parses them out of SolverRunner's captured stderr lines, which already
// print exactly this: include/kopruq/io/Cli.hpp's reportMidasImport()). Nothing here is a fixed/
// fabricated value -- every field has an explicit "has it" flag, and any field the importer's
// report genuinely does not provide (MIDAS load combinations are not supported at all) always
// shows "N/A" rather than a guess.
#include <QGroupBox>
#include <QLabel>

class ModelInfoPanel : public QGroupBox {
  Q_OBJECT
public:
  struct Info {
    QString modelName;   bool hasModelName{false};
    QString unitSystem;  bool hasUnitSystem{false};
    long nodeCount{};        bool hasNodeCount{false};
    long elementCount{};     bool hasElementCount{false};
    long materialCount{};    bool hasMaterialCount{false};
    long sectionCount{};     bool hasSectionCount{false};
    long loadCaseCount{};    bool hasLoadCaseCount{false};
    long combinationCount{}; bool hasCombinationCount{false}; // MIDAS combinations: never set (unsupported)
    long elasticLinkCount{}; bool hasElasticLinkCount{false};
    double extentX{}, extentY{}, extentZ{}; bool hasExtents{false}; // bounding-box size (max-min), metres
  };

  explicit ModelInfoPanel(QWidget* parent = nullptr);

  void setData(const Info& info);
  void setStatus(const QString& text, bool isError);
  // Back to all-placeholder, status banner hidden -- called whenever the MIDAS model selection
  // itself changes, since any previously-shown info belonged to a now-superseded file.
  void reset();

private:
  QLabel* modelNameValue_{};
  QLabel* unitSystemValue_{};
  QLabel* nodeCountValue_{};
  QLabel* elementCountValue_{};
  QLabel* materialCountValue_{};
  QLabel* sectionCountValue_{};
  QLabel* loadCaseCountValue_{};
  QLabel* combinationCountValue_{};
  QLabel* elasticLinkCountValue_{};
  QLabel* extentsValue_{};
  QLabel* statusBanner_{};
};
