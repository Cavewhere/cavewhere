/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Qt includes
#include <QLoggingCategory>
#include <QMetaObject>
#include <QUndoCommand>

//Std includes
#include <algorithm>
#include <cmath>
#include <limits>

//Our includes
#include "cwSketch.h"
#include "cwSketchData.h"
#include "cwSketchViewState.h"
#include "cwPenStrokeModel.h"
#include "cwScale.h"
#include "cwKeywordModel.h"
#include "cwAbstractScrapViewMatrix.h"
#include "cwPlanScrapViewMatrix.h"
#include "cwMatrix4x4Artifact.h"
#include "cwSketchPainter.h"
#include "cwSketchScrapRasterizer.h"
#include "cwSurvey2DGeometryRule.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurveyNetworkArtifact.h"

namespace {

// Off by default. Enable with QT_LOGGING_RULES="cw.sketch.continuation.debug=true"
// (or add to qtlogging.ini). Covers findContinuationTarget + armProbation +
// the per-sample probation hit-test so we can confirm the thresholds match
// the rendered stroke on screen.
Q_LOGGING_CATEGORY(lcContinuation, "cw.sketch.continuation", QtInfoMsg)

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
    emit activeDrawingChanged(true);
    return row;
}

void cwSketch::appendPoint(int strokeIndex, const cwPenPoint &p)
{
    if (strokeIndex < 0 || strokeIndex >= m_strokes.size()) {
        return;
    }

    using Phase = ContinuationState::Phase;
    auto &cs = m_continuationState;

    // Probation phase: hit-test, accumulate travel, decide commit/reject
    // when the window closes. The probation row owns the raw sample either
    // way (committed → row gets removed; rejected → row keeps it).
    if (cs.phase == Phase::Probation && cs.strokeIndex == strokeIndex) {
        const QPointF rawWorld = p.position;
        if (!cs.haveProbationStart) {
            cs.probationStartWorld = rawWorld;
            cs.haveProbationStart = true;
        }
        if (cs.haveLastRawWorld) {
            const QPointF delta = rawWorld - cs.lastRawWorld;
            cs.travelMeters += std::hypot(delta.x(), delta.y());
        }
        cs.lastRawWorld = rawWorld;
        cs.haveLastRawWorld = true;

        // Hit-test against the candidate centerline.
        if (cs.candidateIndex >= 0 && cs.candidateIndex < m_strokes.size()) {
            const cwPenStroke &cand = m_strokes[cs.candidateIndex];
            const double rSq = cs.hitThresholdMeters * cs.hitThresholdMeters;
            int   bestSeg = -1;
            double bestSegDistSq = std::numeric_limits<double>::infinity();
            double sceneBestDistSq = std::numeric_limits<double>::infinity();
            for (int i = 1; i < cand.points.size(); ++i) {
                const QPointF a = cand.points[i - 1].position;
                const QPointF b = cand.points[i].position;
                const double dSq = distancePointToSegmentSquared(rawWorld, a, b);
                if (dSq < sceneBestDistSq) {
                    sceneBestDistSq = dSq;
                }
                if (dSq <= rSq && dSq < bestSegDistSq) {
                    bestSegDistSq = dSq;
                    bestSeg = i;
                }
            }
            cs.sampleCount += 1;
            const double mapScaleRatioLog = m_mapScale ? m_mapScale->scale() : 0.0;
            const double paperPpmLog = cwSketchPainter::pixelsPerMeterFromPaperScale(
                mapScaleRatioLog, cwSketchScrapRasterizer::kTargetDPI);
            const double pxPerMeter = m_viewState
                ? m_viewState->pixelsPerMeter() : 0.0;
            const double minDist = std::sqrt(sceneBestDistSq);
            qCInfo(lcContinuation,
                   "probation sample %d pen=(%.4f, %.4f) "
                   "minDistWorld=%.5f (=%.4f paperPx, %.4f screenPx) "
                   "thresholdWorld=%.5f → %s (hits=%d/%d)",
                   cs.sampleCount, rawWorld.x(), rawWorld.y(),
                   minDist, minDist * paperPpmLog, minDist * pxPerMeter,
                   cs.hitThresholdMeters,
                   (bestSeg >= 0 ? "HIT" : "miss"),
                   cs.hitCount + (bestSeg >= 0 ? 1 : 0), cs.sampleCount);
            if (bestSeg >= 0) {
                cs.hitCount += 1;
                const QPointF a = cand.points[bestSeg - 1].position;
                const QPointF b = cand.points[bestSeg].position;
                const QPointF ab = b - a;
                const double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
                QPointF proj = a;
                QPointF tangent(1.0, 0.0);
                if (lenSq > 0.0) {
                    const QPointF ap = rawWorld - a;
                    const double t = std::clamp(
                        (ap.x() * ab.x() + ap.y() * ab.y()) / lenSq, 0.0, 1.0);
                    proj = a + t * ab;
                    const double len = std::sqrt(lenSq);
                    tangent = QPointF(ab.x() / len, ab.y() / len);
                }

                // First hit anchors the direction reference used at commit
                // *and* the start of the overlap region for the in-probation
                // blend.
                if (cs.firstHitSegmentIndex < 0) {
                    cs.firstHitSegmentIndex = bestSeg;
                    cs.firstHitTangent = tangent;
                    cs.firstHitWorld = proj;
                }
                // Track the extreme in-proximity hit on both ends. Using >=
                // (not >) so a lone hit populates both forward and backward.
                if (cs.furthestForwardSeg < 0 || bestSeg >= cs.furthestForwardSeg) {
                    cs.furthestForwardSeg = bestSeg;
                    cs.furthestForwardWorld = proj;
                    cs.furthestForwardTangent = tangent;
                }
                if (cs.furthestBackwardSeg < 0 || bestSeg <= cs.furthestBackwardSeg) {
                    cs.furthestBackwardSeg = bestSeg;
                    cs.furthestBackwardWorld = proj;
                    cs.furthestBackwardTangent = tangent;
                }
            }
        }

        // Append raw point to the probation row first; the commit path
        // below removes the row wholesale, so the in-progress sample is
        // discarded on commit and preserved on reject.
        m_strokes[strokeIndex].points.append(p);
        scheduleDirtyEmit(strokeIndex);

        // Window closed? Decide.
        const bool windowClosed = cs.probationWindowMeters > 0.0
            && cs.travelMeters >= cs.probationWindowMeters;
        if (!windowClosed) {
            return;
        }

        const double rate = cs.sampleCount > 0
            ? static_cast<double>(cs.hitCount) / cs.sampleCount
            : 0.0;
        qCInfo(lcContinuation,
               "probation window closed: travel=%.5f/%.5f m "
               "rate=%.2f (%d/%d) threshold=%.2f "
               "firstSeg=%d fwdSeg=%d bwdSeg=%d",
               cs.travelMeters, cs.probationWindowMeters,
               rate, cs.hitCount, cs.sampleCount,
               cs.commitHitRateThreshold,
               cs.firstHitSegmentIndex,
               cs.furthestForwardSeg, cs.furthestBackwardSeg);
        const bool commit = (rate >= cs.commitHitRateThreshold)
            && cs.candidateIndex >= 0
            && cs.candidateIndex < m_strokes.size()
            && cs.firstHitSegmentIndex >= 0;

        if (!commit) {
            qCInfo(lcContinuation, "  → REJECT");
            const int probationIdx = cs.strokeIndex;
            m_continuationState = ContinuationState();
            emit continuationRejected();
            (void)probationIdx;
            return;
        }

        // Resolve direction: project the pen's net motion during probation
        // onto the first-hit tangent. Positive → user extends along the
        // stroke's stored direction (forward commit). Negative → user
        // extends against it (reverse the candidate so its original head
        // becomes the new tail, then blend over the overlap normally).
        const QPointF motion = cs.lastRawWorld - cs.probationStartWorld;
        const double motionDotTangent =
            motion.x() * cs.firstHitTangent.x()
            + motion.y() * cs.firstHitTangent.y();
        const bool backward = motionDotTangent < 0.0;

        const int probationIdx = cs.strokeIndex;
        const int candidateIdxBefore = cs.candidateIndex;

        // The overlap region on the candidate runs from firstHitWorld (where
        // the pen first touched the stroke) to overlapEnd (the *furthest*
        // in-proximity hit in the chosen direction). That region's old
        // polyline gets discarded; replacement points come from the buffered
        // probation samples, each lerped against the straight-line chord
        // (firstHit → overlapEnd) with weight t = clamp(sampleTravel /
        // probationWindow, 0, 1). A small t favours the old centerline
        // (smoothing out early jitter); t near 1 favours the raw pen.
        const QPointF overlapStart = cs.firstHitWorld;
        QPointF       overlapEnd;
        if (backward) {
            overlapEnd = cs.furthestBackwardWorld;
        } else {
            overlapEnd = cs.furthestForwardWorld;
        }

        // Snapshot probation samples + per-sample cumulative travel *before*
        // removing the probation row.
        QVector<cwPenPoint> probSamples = m_strokes[probationIdx].points;
        QVector<double>     sampleTravel(probSamples.size(), 0.0);
        {
            double acc = 0.0;
            QPointF prev;
            bool havePrev = false;
            for (int i = 0; i < probSamples.size(); ++i) {
                if (havePrev) {
                    const QPointF d = probSamples[i].position - prev;
                    acc += std::hypot(d.x(), d.y());
                }
                sampleTravel[i] = acc;
                prev = probSamples[i].position;
                havePrev = true;
            }
        }

        qCInfo(lcContinuation,
               "  → COMMIT: %s (motion·firstTangent=%.4f) "
               "overlapStart=(%.4f, %.4f) overlapEnd=(%.4f, %.4f) "
               "probSamples=%d",
               backward ? "BACKWARD (reversing candidate)" : "FORWARD",
               motionDotTangent, overlapStart.x(), overlapStart.y(),
               overlapEnd.x(), overlapEnd.y(),
               static_cast<int>(probSamples.size()));

        m_strokeModel->beginRemoveRows(QModelIndex(), probationIdx, probationIdx);
        m_strokes.removeAt(probationIdx);
        m_strokeModel->endRemoveRows();
        // m_pendingDirtyRow may have referenced the now-gone row; clear it.
        m_pendingDirtyRow = -1;

        // Removing a row before the candidate would shift its index down.
        int candidateIdx = candidateIdxBefore;
        if (probationIdx < candidateIdxBefore) {
            candidateIdx -= 1;
        }
        if (candidateIdx < 0 || candidateIdx >= m_strokes.size()) {
            // Safety net: candidate vanished. Nothing to commit to.
            m_continuationState = ContinuationState();
            emit continuationRejected();
            return;
        }

        cwPenStroke &cand = m_strokes[candidateIdx];

        // prefixCount points of the (possibly reversed) candidate survive as
        // the untouched leading section. In forward mode this is everything
        // before segment firstHitSegmentIndex. In backward mode, once we
        // reverse the array, the first-hit segment's upper reversed-frame
        // index is N − firstHitSegmentIndex, so the pre-overlap prefix has
        // that many points.
        int prefixCount;
        if (backward) {
            std::reverse(cand.points.begin(), cand.points.end());
            prefixCount = cand.points.size() - cs.firstHitSegmentIndex;
        } else {
            prefixCount = cs.firstHitSegmentIndex;
        }
        prefixCount = std::clamp<int>(prefixCount, 0, cand.points.size());

        QVector<cwPenPoint> newPoints;
        newPoints.reserve(prefixCount + probSamples.size());
        for (int i = 0; i < prefixCount; ++i) {
            newPoints.append(cand.points[i]);
        }

        const double window = cs.probationWindowMeters;
        for (int i = 0; i < probSamples.size(); ++i) {
            const double t = (window > 0.0)
                ? std::clamp(sampleTravel[i] / window, 0.0, 1.0)
                : 1.0;
            const QPointF oldLine(
                (1.0 - t) * overlapStart.x() + t * overlapEnd.x(),
                (1.0 - t) * overlapStart.y() + t * overlapEnd.y());
            const QPointF raw = probSamples[i].position;
            cwPenPoint bp = probSamples[i];
            bp.position = QPointF(
                (1.0 - t) * oldLine.x() + t * raw.x(),
                (1.0 - t) * oldLine.y() + t * raw.y());
            newPoints.append(bp);
        }
        cand.points = newPoints;

        // Reset the model around the candidate replacement; structural
        // changes to the polyline are not reliably consumed via dataChanged
        // alone.
        applyStrokes(m_strokes);

        // Back to Off — subsequent appendPoint() calls store raw. `used`
        // persists through to endStroke() so the undo label is "Continue
        // Stroke".
        m_continuationState = ContinuationState();
        m_continuationState.used = true;
        m_continuationState.strokeIndex = candidateIdx;

        emit continuationCommitted(candidateIdx);
        return;
    }

    m_strokes[strokeIndex].points.append(p);
    scheduleDirtyEmit(strokeIndex);
}

void cwSketch::scheduleDirtyEmit(int row)
{
    if (m_pendingDirtyRow != -1) {
        return;
    }
    m_pendingDirtyRow = row;
    QMetaObject::invokeMethod(this, [this]{
        const int r = m_pendingDirtyRow;
        m_pendingDirtyRow = -1;
        if (r < 0 || r >= m_strokes.size()) {
            return;
        }
        const QModelIndex idx = m_strokeModel->index(r);
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
    const QString label = m_continuationState.used
        ? QStringLiteral("Continue Stroke")
        : QStringLiteral("Draw Stroke");
    auto *cmd = new cwSketchSetStrokesCommand(this, m_startStrokes, m_strokes, label);
    m_startStrokes.clear();
    m_continuationState = ContinuationState();
    m_undoStack->push(cmd);
    emit strokeEnded();
    emit activeDrawingChanged(false);
}

cwSketchContinuationTarget cwSketch::findContinuationTarget(
    cwPenStroke::Kind kind, QPointF worldPoint) const
{
    cwSketchContinuationTarget result;
    if (kind == cwPenStroke::Eraser) {
        return result;
    }
    if (!m_viewState) {
        return result;
    }
    // Strokes are rendered at a fixed *world* thickness, set by
    // paperStrokePenScale in cwSketchCanvasRenderer. That pen scale uses
    // kTargetDPI (200), NOT the screen's pixelDensity — so the rendered
    // world half-width is 0.5 × stroke.width / paperPpm, zoom-independent.
    // Earlier versions divided by viewState->pixelsPerMeterPaper() (m11)
    // which folds in screen pixelDensity, giving a threshold that only
    // coincidentally matched the rendered edge on ~100 DPI screens.
    const double mapScaleRatio = m_mapScale ? m_mapScale->scale() : 0.0;
    const double paperPpm = cwSketchPainter::pixelsPerMeterFromPaperScale(
        mapScaleRatio, cwSketchScrapRasterizer::kTargetDPI);
    const double pxPerMeter = m_viewState->pixelsPerMeter();
    // Require the view matrix too — not needed for the threshold math
    // itself, but pen input only flows through this path when the view
    // is wired up, and it keeps parity with armProbation's requirements.
    if (paperPpm <= 0.0 || pxPerMeter <= 0.0) {
        return result;
    }
    qCInfo(lcContinuation,
           "findContinuationTarget: kind=%d pen=(%.4f, %.4f) "
           "paperPpm=%.4f pxPerMeter=%.4f zoom=%.4f strokes=%d",
           static_cast<int>(kind), worldPoint.x(), worldPoint.y(),
           paperPpm, pxPerMeter,
           m_viewState->zoom(),
           static_cast<int>(m_strokes.size()));

    double bestDistSq = std::numeric_limits<double>::infinity();

    for (int sIdx = 0; sIdx < m_strokes.size(); ++sIdx) {
        const cwPenStroke &stroke = m_strokes[sIdx];
        if (stroke.kind != kind) {
            continue;
        }
        if (stroke.points.size() < 2) {
            continue;
        }

        // Threshold = half the *rendered* world width = 0.5 × stroke.width
        // / paperPpm. Matches the outer edge of the visible stroke. Use
        // 0.25 for inner-half only if edge-grazes feel wrong.
        const double thresholdMeters = 0.5 * stroke.width / paperPpm;
        // Equivalent in paper/screen pixels, for logs only.
        const double thresholdPaperPx = thresholdMeters * paperPpm;
        const double thresholdSq = thresholdMeters * thresholdMeters;

        const QRectF bbox = stroke.boundingBox().adjusted(
            -thresholdMeters, -thresholdMeters, thresholdMeters, thresholdMeters);
        if (!bbox.contains(worldPoint)) {
            continue;
        }

        double strokeBestDistSq = std::numeric_limits<double>::infinity();
        for (int i = 1; i < stroke.points.size(); ++i) {
            const QPointF a = stroke.points[i - 1].position;
            const QPointF b = stroke.points[i].position;
            const double dSq = distancePointToSegmentSquared(worldPoint, a, b);
            if (dSq < strokeBestDistSq) {
                strokeBestDistSq = dSq;
            }
            if (dSq > thresholdSq || dSq >= bestDistSq) {
                continue;
            }
            bestDistSq = dSq;
            result.strokeIndex = sIdx;
            result.strokeWidth = stroke.width;
        }
        const double minDist = std::sqrt(strokeBestDistSq);
        qCInfo(lcContinuation,
               "  stroke[%d] kind=%d width=%.4f thresholdWorld=%.5f "
               "thresholdPaperPx=%.4f thresholdScreenPx=%.4f "
               "minDistWorld=%.5f minDistPaperPx=%.4f minDistScreenPx=%.4f %s",
               sIdx, static_cast<int>(stroke.kind), stroke.width,
               thresholdMeters, thresholdPaperPx, thresholdMeters * pxPerMeter,
               minDist, minDist * paperPpm, minDist * pxPerMeter,
               (strokeBestDistSq <= thresholdSq ? "HIT" : "miss"));
    }

    qCInfo(lcContinuation, "findContinuationTarget → strokeIndex=%d strokeWidth=%.4f",
           result.strokeIndex, result.strokeWidth);
    return result;
}

void cwSketch::armProbation(int probationStrokeIndex,
                            cwSketchContinuationTarget candidate,
                            double probationWindowScreenPx,
                            double commitHitRateThreshold)
{
    if (probationStrokeIndex < 0 || probationStrokeIndex >= m_strokes.size()) {
        return;
    }
    if (candidate.strokeIndex < 0 || candidate.strokeIndex >= m_strokes.size()) {
        return;
    }
    if (candidate.strokeIndex == probationStrokeIndex) {
        // Cannot graft onto self; the just-created probation row has no
        // segments anyway.
        return;
    }
    if (!m_viewState) {
        return;
    }
    // Two distinct frames:
    //   paperPpm (zoom-independent, fixed via kTargetDPI) drives the hit
    //     threshold so it matches the rendered stroke's world width —
    //     same logic as findContinuationTarget above. Screen pixelDensity
    //     must not feed in here because the rendered stroke width is
    //     computed with paperStrokePenScale, not the screen matrix.
    //   pxPerMeter (zoom-aware, screen-space) drives the probation window —
    //     a pen-travel distance the user drags on screen ("1 cm of drag")
    //     that should stay a fixed screen distance regardless of zoom.
    const double mapScaleRatio = m_mapScale ? m_mapScale->scale() : 0.0;
    const double paperPpm = cwSketchPainter::pixelsPerMeterFromPaperScale(
        mapScaleRatio, cwSketchScrapRasterizer::kTargetDPI);
    const double pxPerMeter = m_viewState->pixelsPerMeter();
    if (paperPpm <= 0.0 || pxPerMeter <= 0.0) {
        return;
    }

    m_continuationState = ContinuationState();
    m_continuationState.phase = ContinuationState::Phase::Probation;
    m_continuationState.strokeIndex = probationStrokeIndex;
    m_continuationState.candidateIndex = candidate.strokeIndex;
    m_continuationState.hitThresholdMeters =
        (0.5 * candidate.strokeWidth) / paperPpm;
    m_continuationState.probationWindowMeters =
        probationWindowScreenPx / pxPerMeter;
    m_continuationState.commitHitRateThreshold = commitHitRateThreshold;

    qCInfo(lcContinuation,
           "armProbation: probationRow=%d candidateRow=%d candidateWidth=%.4f "
           "hitThresholdWorld=%.5f (=%.4f paperPx, %.4f screenPx) "
           "probationWindowWorld=%.5f hitRateThreshold=%.2f",
           probationStrokeIndex, candidate.strokeIndex, candidate.strokeWidth,
           m_continuationState.hitThresholdMeters,
           m_continuationState.hitThresholdMeters * paperPpm,
           m_continuationState.hitThresholdMeters * pxPerMeter,
           m_continuationState.probationWindowMeters,
           commitHitRateThreshold);
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
