/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QMetaObject>
#include <QUndoCommand>

//Our includes
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwPenStrokeModel.h"
#include "cwScale.h"
#include "cwKeywordModel.h"
#include "cwAbstractScrapViewMatrix.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwMatrix4x4Artifact.h"
#include "cwSurvey2DGeometryRule.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurveyNetworkArtifact.h"

namespace {

// QUndoStack::push() runs redo() once on push. Since endStroke()/clearStrokes()
// apply the mutation *before* pushing, the first redo() would otherwise tear
// the model down via a full resetModel. Skip the first redo to preserve
// downstream delegate state on stroke completion.
class cwSketchSetStrokesCommand : public QUndoCommand
{
public:
    cwSketchSetStrokesCommand(cwSketch *sketch,
                              const QVector<cwPenStroke> &before,
                              const QVector<cwPenStroke> &after,
                              const QString &text)
        : QUndoCommand(text, nullptr),
          m_sketch(sketch),
          m_before(before),
          m_after(after)
    {}

    void undo() override { m_sketch->setStrokes(m_before); }
    void redo() override {
        if (m_firstRun) {
            m_firstRun = false;
            return;
        }
        m_sketch->setStrokes(m_after);
    }

private:
    cwSketch *m_sketch;
    QVector<cwPenStroke> m_before;
    QVector<cwPenStroke> m_after;
    bool m_firstRun = true;
};

} // namespace

cwSketch::cwSketch(QObject *parent)
    : QObject(parent),
      m_id(QUuid::createUuid()),
      m_mapScale(new cwScale(this)),
      m_strokeModel(new cwPenStrokeModel(this)),
      m_undoStack(new QUndoStack(this)),
      m_keywordModel(new cwKeywordModel(this)),
      m_matrixArtifact(new cwMatrix4x4Artifact(this)),
      m_geometryRule(new cwSurvey2DGeometryRule(this))
{
    m_undoStack->setUndoLimit(32);

    rebuildViewMatrixForType();
    syncMatrixArtifact();
    m_geometryRule->setViewMatrix(m_matrixArtifact);
}

cwSketch::~cwSketch() = default;

void cwSketch::setParentTrip(cwTrip *trip)
{
    m_parentTrip = trip;
}

void cwSketch::setName(const QString &name)
{
    if (m_name == name) {
        return;
    }
    m_name = name;
    emit nameChanged();
}

void cwSketch::setId(const QUuid &id)
{
    m_id = id.isNull() ? QUuid::createUuid() : id;
}

void cwSketch::setViewType(ViewType type)
{
    if (m_viewType == type) {
        return;
    }
    m_viewType = type;
    rebuildViewMatrixForType();
    syncMatrixArtifact();
    emit viewTypeChanged();
}

cwSurveyNetworkArtifact *cwSketch::surveyNetworkArtifact() const
{
    return m_geometryRule->surveyNetwork();
}

void cwSketch::setSurveyNetworkArtifact(cwSurveyNetworkArtifact *artifact)
{
    if (m_geometryRule->surveyNetwork() == artifact) {
        return;
    }
    m_geometryRule->setSurveyNetwork(artifact);
    emit surveyNetworkArtifactChanged();
}

cwSurvey2DGeometryArtifact *cwSketch::survey2DGeometry() const
{
    return m_geometryRule->survey2DGeometry();
}

void cwSketch::rebuildViewMatrixForType()
{
    if (m_viewType != Plan) {
        qWarning("cwSketch: non-Plan view types are not supported yet; "
                 "falling back to Plan for sketch \"%s\"",
                 qPrintable(m_name));
    }

    delete m_viewMatrix;
    m_viewMatrix = new cwPlanScrapViewMatrix(this);

    connect(m_viewMatrix, &cwAbstractScrapViewMatrix::matrixChanged,
            this, &cwSketch::syncMatrixArtifact);
}

void cwSketch::syncMatrixArtifact()
{
    if (m_viewMatrix && m_matrixArtifact) {
        m_matrixArtifact->setMatrix4x4(m_viewMatrix->matrix());
    }
}

void cwSketch::setIconImagePath(const QString &path)
{
    if (m_iconImagePath == path) {
        return;
    }
    m_iconImagePath = path;
    emit iconImagePathChanged();
}

void cwSketch::setAnchorStation(const QString &name)
{
    if (m_anchorStation == name) {
        return;
    }
    m_anchorStation = name;
    emit anchorStationChanged();
}

void cwSketch::setStrokes(const QVector<cwPenStroke> &strokes)
{
    applyStrokes(strokes);
}

int cwSketch::beginStroke(cwPenStroke::Kind kind, double width, const QColor &color)
{
    m_startStrokes = m_strokes;

    cwPenStroke stroke;
    stroke.kind  = kind;
    stroke.width = width;
    stroke.color = color;
    stroke.id    = QUuid::createUuid();

    const int row = m_strokes.size();
    m_strokeModel->beginInsertRows(QModelIndex(), row, row);
    m_strokes.append(stroke);
    m_strokeModel->endInsertRows();
    return row;
}

void cwSketch::appendPoint(int strokeIndex, const cwPenPoint &p)
{
    if (strokeIndex < 0 || strokeIndex >= m_strokes.size()) {
        return;
    }
    m_strokes[strokeIndex].points.append(p);
    if (m_pendingDirtyRow != -1) {
        return;
    }
    m_pendingDirtyRow = strokeIndex;
    QMetaObject::invokeMethod(this, [this]{
        const int row = m_pendingDirtyRow;
        m_pendingDirtyRow = -1;
        const QModelIndex idx = m_strokeModel->index(row);
        emit m_strokeModel->dataChanged(idx, idx,
            { cwPenStrokeModel::PointsRole, cwPenStrokeModel::StrokeRole });
    }, Qt::QueuedConnection);
}

void cwSketch::appendPoint(int strokeIndex, QPointF position, double pressure, qint64 timestampMs)
{
    appendPoint(strokeIndex, cwPenPoint(position, pressure, timestampMs));
}

void cwSketch::endStroke()
{
    auto *cmd = new cwSketchSetStrokesCommand(this, m_startStrokes, m_strokes, "Draw Stroke");
    m_startStrokes.clear();
    m_undoStack->push(cmd);
    emit strokeEnded();
}

void cwSketch::clearStrokes()
{
    if (m_strokes.isEmpty()) {
        return;
    }
    const auto before = m_strokes;
    applyStrokes({});
    m_undoStack->push(new cwSketchSetStrokesCommand(this, before, {}, "Clear Strokes"));
}

void cwSketch::applyStrokes(const QVector<cwPenStroke> &strokes)
{
    m_strokeModel->beginResetModel();
    m_strokes = strokes;
    m_strokeModel->endResetModel();
    emit strokesReset();
}

cwSketchData cwSketch::data() const
{
    cwSketchData d;
    d.name          = m_name;
    d.id            = m_id;
    d.viewType      = static_cast<cwSketchData::ViewType>(m_viewType);
    d.mapScale      = m_mapScale->data();
    d.strokes       = m_strokes;
    d.anchorStation = m_anchorStation;
    return d;
}

void cwSketch::setData(const cwSketchData &d)
{
    setName(d.name);
    setId(d.id);
    setViewType(static_cast<ViewType>(d.viewType));
    m_mapScale->setData(d.mapScale);
    applyStrokes(d.strokes);
    setAnchorStation(d.anchorStation);
    m_undoStack->clear();
}
