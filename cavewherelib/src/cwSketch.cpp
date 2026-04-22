/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QMetaObject>
#include <QUndoCommand>

//Std includes
#include <algorithm>

//Our includes
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwSketchViewState.h"
#include "cwPenStrokeModel.h"
#include "cwPenStrokeFilter.h"
#include "cwScale.h"
#include "cwKeywordModel.h"
#include "cwAbstractScrapViewMatrix.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwMatrix4x4Artifact.h"
#include "cwSurvey2DGeometryRule.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurveyNetworkArtifact.h"
#include "cwSketchSettings.h"

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

// Per-pointer-move erase commands from one drag collapse into a single undo
// step via QUndoStack::tryMerge — same session id means merge.
class cwSketchEraseCommand : public QUndoCommand
{
public:
    static constexpr int Id = 0x5A5E;

    cwSketchEraseCommand(cwSketch *sketch,
                         const QVector<cwPenStroke> &before,
                         const QVector<cwPenStroke> &after,
                         int session)
        : QUndoCommand(QStringLiteral("Erase"), nullptr),
          m_sketch(sketch),
          m_before(before),
          m_after(after),
          m_session(session)
    {}

    int id() const override { return Id; }

    bool mergeWith(const QUndoCommand *other) override {
        const auto *o = static_cast<const cwSketchEraseCommand *>(other);
        if (o->m_sketch != m_sketch || o->m_session != m_session) {
            return false;
        }
        m_after = o->m_after;
        return true;
    }

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
    int m_session;
    bool m_firstRun = true;
};

double distancePointToSegmentSquared(const QPointF &p, const QPointF &a, const QPointF &b)
{
    const QPointF ab = b - a;
    const double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
    if (lenSq <= 0.0) {
        const QPointF d = p - a;
        return d.x() * d.x() + d.y() * d.y();
    }
    const QPointF ap = p - a;
    double t = (ap.x() * ab.x() + ap.y() * ab.y()) / lenSq;
    t = std::clamp(t, 0.0, 1.0);
    const QPointF proj = a + t * ab;
    const QPointF d = p - proj;
    return d.x() * d.x() + d.y() * d.y();
}

bool pointWithinPath(const QPointF &p,
                     const QVector<QPointF> &path,
                     double radiusSquared)
{
    if (path.isEmpty()) {
        return false;
    }
    if (path.size() == 1) {
        const QPointF d = p - path.first();
        return (d.x() * d.x() + d.y() * d.y()) <= radiusSquared;
    }
    for (int i = 1; i < path.size(); ++i) {
        if (distancePointToSegmentSquared(p, path[i - 1], path[i]) <= radiusSquared) {
            return true;
        }
    }
    return false;
}

struct ErasedStrokes {
    QVector<cwPenStroke> strokes;
    bool changed = false;
};

ErasedStrokes computeErasedStrokes(const QVector<cwPenStroke> &strokes,
                                   const QVector<QPointF> &path,
                                   double radius)
{
    const double r2 = radius * radius;
    const QRectF pathBox =
        cwBoundingBoxOf(path, [](const QPointF &p) { return p; })
            .adjusted(-radius, -radius, radius, radius);
    ErasedStrokes out;
    out.strokes.reserve(strokes.size());

    for (const cwPenStroke &src : strokes) {
        // Manual overlap test — QRectF::intersects treats degenerate (zero
        // width/height) rects as empty, which would drop purely horizontal
        // or vertical strokes.
        const QRectF strokeBox = src.boundingBox();
        const bool disjoint =
            strokeBox.right()  < pathBox.left()  ||
            pathBox.right()    < strokeBox.left() ||
            strokeBox.bottom() < pathBox.top()   ||
            pathBox.bottom()   < strokeBox.top();
        if (disjoint) {
            out.strokes.append(src);
            continue;
        }
        QVector<cwPenPoint> run;
        run.reserve(src.points.size());
        bool anyErased = false;
        QVector<cwPenStroke> subStrokes;

        auto flushRun = [&](bool preserveId) {
            if (run.size() >= 2) {
                cwPenStroke fragment;
                fragment.kind   = src.kind;
                fragment.width  = src.width;
                fragment.color  = src.color;
                fragment.id     = preserveId ? src.id : QUuid::createUuid();
                fragment.points = run;
                subStrokes.append(fragment);
            } else if (!run.isEmpty()) {
                // A single surviving point cannot render a line; dropping
                // it counts as a stroke-level change so the caller records
                // one undo step for the shrinkage.
                anyErased = true;
            }
            run.clear();
        };

        for (const cwPenPoint &pt : src.points) {
            if (pointWithinPath(pt.position, path, r2)) {
                anyErased = true;
                flushRun(false);
            } else {
                run.append(pt);
            }
        }
        flushRun(!anyErased);

        out.strokes.append(subStrokes);
        if (anyErased) {
            out.changed = true;
        }
    }
    return out;
}

} // namespace

cwSketch::cwSketch(QObject *parent)
    : QObject(parent),
      m_id(QUuid::createUuid()),
      m_mapScale(new cwScale(this)),
      m_strokeModel(new cwPenStrokeModel(this)),
      m_undoStack(new QUndoStack(this)),
      m_keywordModel(new cwKeywordModel(this)),
      m_viewState(new cwSketchViewState(this)),
      m_matrixArtifact(new cwMatrix4x4Artifact(this)),
      m_geometryRule(new cwSurvey2DGeometryRule(this))
{
    m_undoStack->setUndoLimit(32);

    // Default paper scale 1:250 — matches SketchItem.qml's view-matrix default.
    m_mapScale->scaleDenominator()->setValue(250.0);

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
    // Opt-in trim of Apple Pencil start/end hooks. We run this before the
    // undo command captures the final stroke so undo/redo treats the
    // cleaned polyline as one atomic unit; previously-committed strokes
    // are unaffected if the setting is toggled later.
    auto *settings = cwSketchSettings::instance();
    const bool filterOn = settings && settings->filterPenHooks();

    if (!m_strokes.isEmpty()) {
        const int row = m_strokes.size() - 1;
        cwPenStroke &stroke = m_strokes[row];

        // Dump the raw captured polyline in a makeStroke({...})-compatible
        // form so real pen data can be pasted straight into a test case.
        // Always dumped (category-gated) regardless of the filter setting
        // so you can see what the tablet produced before any trimming.
        if (lcPenFilter().isDebugEnabled()) {
            QString raw;
            raw.reserve(stroke.points.size() * 32);
            for (const cwPenPoint &p : stroke.points) {
                raw += QString::asprintf("    {%.6f, %.6f},\n",
                                         p.position.x(), p.position.y());
            }
            qCDebug(lcPenFilter).noquote()
                << QString("stroke end: kind=%1 width=%2 points=%3 filter=%4\nraw points:\n%5")
                       .arg(int(stroke.kind))
                       .arg(stroke.width)
                       .arg(stroke.points.size())
                       .arg(filterOn ? "on" : "off")
                       .arg(raw);
        }

        if (filterOn) {
            const QVector<cwPenPoint> trimmed = cwPenStrokeFilter::trimHooks(stroke.points);
            const int removed = stroke.points.size() - trimmed.size();
            if (lcPenFilter().isDebugEnabled()) {
                qCDebug(lcPenFilter) << "trimHooks removed" << removed
                                     << "point(s); kept" << trimmed.size();
            }
            if (removed > 0) {
                stroke.points = trimmed;
                // The append-time coalescer only flushes during the
                // drag; after the last appendPoint we must emit our own
                // dataChanged so the model/view sees the trim.
                const QModelIndex idx = m_strokeModel->index(row);
                emit m_strokeModel->dataChanged(idx, idx,
                    { cwPenStrokeModel::PointsRole, cwPenStrokeModel::StrokeRole });
            }
        }
    }

    auto *cmd = new cwSketchSetStrokesCommand(this, m_startStrokes, m_strokes, "Draw Stroke");
    m_startStrokes.clear();
    m_undoStack->push(cmd);
    emit strokeEnded();
}

void cwSketch::eraseAlongPath(const QVector<QPointF> &pathPointsWorld, double radiusWorld)
{
    if (pathPointsWorld.isEmpty() || radiusWorld <= 0.0 || m_strokes.isEmpty()) {
        return;
    }

    auto result = computeErasedStrokes(m_strokes, pathPointsWorld, radiusWorld);
    if (!result.changed) {
        return;
    }

    // Snapshot only once we know the erase actually mutates — on the hot path
    // of a hovering eraser this avoids a per-frame deep copy of all strokes.
    const auto before = m_strokes;
    applyStrokes(result.strokes);
    m_undoStack->push(new cwSketchEraseCommand(this, before, result.strokes,
                                               m_eraseSession));
}

void cwSketch::endEraseSession()
{
    ++m_eraseSession;
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
