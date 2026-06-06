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
#include "cwSymbologyPaletteData.h"
#include "cwSymbologyPaletteSeed.h"

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

double distance(const QPointF &a, const QPointF &b)
{
    const QPointF d = b - a;
    return std::hypot(d.x(), d.y());
}

// stored = lerp(oldLine(t), raw, t) where oldLine(t) = lerp(start, end, t).
// Used by both the probation-region replay and the endpoint-blend phase.
QPointF blendSample(const QPointF &start, const QPointF &end,
                    const QPointF &raw, double t)
{
    const QPointF oldLine(
        (1.0 - t) * start.x() + t * end.x(),
        (1.0 - t) * start.y() + t * end.y());
    return QPointF(
        (1.0 - t) * oldLine.x() + t * raw.x(),
        (1.0 - t) * oldLine.y() + t * raw.y());
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
                fragment.brushName = src.brushName;
                fragment.id        = preserveId ? src.id : QUuid::createUuid();
                fragment.points    = run;
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

void cwSketch::ContinuationState::accumulateTravel(const QPointF &raw)
{
    if (haveLastRawWorld) {
        travelMeters += distance(lastRawWorld, raw);
    }
    lastRawWorld = raw;
    haveLastRawWorld = true;
}

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

    m_paletteSnapshot = cwSymbologyPaletteSeed::create().snapshot();

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

int cwSketch::beginStroke(const QString &brushName)
{
    m_startStrokes = m_strokes;

    cwPenStroke stroke;
    stroke.brushName = brushName;
    stroke.id        = QUuid::createUuid();

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

    if (cs.phase == Phase::EndpointBlend && cs.strokeIndex == strokeIndex) {
        const QPointF raw = p.position;
        cs.accumulateTravel(raw);

        const double window = cs.endpointBlendWindowMeters;
        const double t = (window > 0.0)
            ? std::clamp(cs.travelMeters / window, 0.0, 1.0)
            : 1.0;
        const double arc = t * window;

        const QPointF stored = blendSample(
            cs.endpointBlendStartWorld, cs.endpointBlendEndWorld, raw, t);

        auto &points = m_strokes[strokeIndex].points;
        int removedCount = 0;
        while (!cs.endpointOldTailArcs.isEmpty()
               && cs.endpointOldTailArcs.first() <= arc) {
            points.removeAt(cs.endpointOldTailHeadIdx);
            cs.endpointOldTailArcs.removeFirst();
            ++removedCount;
        }

        cwPenPoint bp = p;
        bp.position = stored;
        points.insert(cs.endpointOldTailHeadIdx, bp);
        cs.endpointOldTailHeadIdx += 1;

        scheduleDirtyEmit(strokeIndex);

        qCInfo(lcContinuation,
               "endpointBlend sample: travel=%.5f/%.5f t=%.3f arc=%.5f "
               "stored=(%.4f, %.4f) removed=%d tailRemaining=%d pts=%d%s",
               cs.travelMeters, window, t, arc,
               stored.x(), stored.y(),
               removedCount,
               static_cast<int>(cs.endpointOldTailArcs.size()),
               static_cast<int>(points.size()),
               (t >= 1.0 ? " → BLEND DONE" : ""));

        if (t >= 1.0) {
            // Sweep stragglers in case floating-point slop left arcs > window.
            while (!cs.endpointOldTailArcs.isEmpty()) {
                points.removeAt(cs.endpointOldTailHeadIdx);
                cs.endpointOldTailArcs.removeFirst();
            }
            cs.phase = Phase::Off;
        }
        return;
    }

    if (cs.phase == Phase::Probation && cs.strokeIndex == strokeIndex) {
        if (!handleProbationSample(strokeIndex, p)) {
            return;
        }

        const double rate = cs.sampleCount > 0
            ? static_cast<double>(cs.hitCount) / cs.sampleCount
            : 0.0;
        if (lcContinuation().isInfoEnabled()) {
            qCInfo(lcContinuation,
                   "probation window closed: travel=%.5f/%.5f m "
                   "rate=%.2f (%d/%d) threshold=%.2f "
                   "firstSeg=%d fwdSeg=%d bwdSeg=%d",
                   cs.travelMeters, cs.probationWindowMeters,
                   rate, cs.hitCount, cs.sampleCount,
                   cs.commitHitRateThreshold,
                   cs.firstHitSegmentIndex,
                   cs.furthestForwardSeg, cs.furthestBackwardSeg);
        }

        const bool canCommit = (rate >= cs.commitHitRateThreshold)
            && cs.candidateIndex >= 0
            && cs.candidateIndex < m_strokes.size()
            && cs.firstHitSegmentIndex >= 0;
        if (!canCommit) {
            qCInfo(lcContinuation, "  → REJECT");
            m_continuationState = ContinuationState();
            emit continuationRejected();
            return;
        }
        commitContinuation();
        return;
    }

    m_strokes[strokeIndex].points.append(p);
    scheduleDirtyEmit(strokeIndex);
}

bool cwSketch::handleProbationSample(int strokeIndex, const cwPenPoint &p)
{
    auto &cs = m_continuationState;
    const QPointF rawWorld = p.position;

    if (!cs.haveProbationStart) {
        cs.probationStartWorld = rawWorld;
        cs.haveProbationStart = true;
    }
    cs.accumulateTravel(rawWorld);

    // AABB early-reject: the candidate doesn't move during probation, so a
    // one-shot bbox-vs-point check skips the per-segment scan whenever the
    // pen is outside the threshold-expanded envelope.
    int   bestSeg = -1;
    double sceneBestDistSq = std::numeric_limits<double>::infinity();
    if (cs.candidateIndex >= 0 && cs.candidateIndex < m_strokes.size()
        && (cs.candidateBbox.isNull() || cs.candidateBbox.contains(rawWorld))) {
        const cwPenStroke &cand = m_strokes[cs.candidateIndex];
        const double rSq = cs.hitThresholdMeters * cs.hitThresholdMeters;
        double bestSegDistSq = std::numeric_limits<double>::infinity();
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
    }
    cs.sampleCount += 1;

    if (lcContinuation().isInfoEnabled()) {
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
    }

    if (bestSeg >= 0) {
        cs.hitCount += 1;
        const cwPenStroke &cand = m_strokes[cs.candidateIndex];
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

        if (cs.firstHitSegmentIndex < 0) {
            cs.firstHitSegmentIndex = bestSeg;
            cs.firstHitTangent = tangent;
            cs.firstHitWorld = proj;
        }
        // Using >= (not >) so a lone hit populates both extremes.
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

    // The probation row owns every raw sample until commit/reject:
    // committed rows are removed wholesale, rejected rows survive as the
    // user's new stroke.
    m_strokes[strokeIndex].points.append(p);
    scheduleDirtyEmit(strokeIndex);

    return cs.probationWindowMeters > 0.0
        && cs.travelMeters >= cs.probationWindowMeters;
}

void cwSketch::commitContinuation()
{
    auto &cs = m_continuationState;

    // Project pen motion onto firstHitTangent. Negative means the user is
    // extending against the candidate's stored direction, so we reverse the
    // candidate — the line is visually identical but its stored array now
    // ends at the user's landing side, which lets the overlap/prefix math
    // below stay single-ended.
    const QPointF motion = cs.lastRawWorld - cs.probationStartWorld;
    const double motionDotTangent =
        motion.x() * cs.firstHitTangent.x()
        + motion.y() * cs.firstHitTangent.y();
    const bool backward = motionDotTangent < 0.0;

    const int probationIdx = cs.strokeIndex;
    const int candidateIdxBefore = cs.candidateIndex;
    const QPointF overlapStart = cs.firstHitWorld;
    const QPointF overlapEnd = backward
        ? cs.furthestBackwardWorld
        : cs.furthestForwardWorld;
    const double window = cs.probationWindowMeters;
    const int firstHitSeg = cs.firstHitSegmentIndex;

    // Snapshot probation samples + per-sample cumulative travel before the
    // probation row is removed.
    QVector<cwPenPoint> probSamples = m_strokes[probationIdx].points;
    QVector<double>     sampleTravel(probSamples.size(), 0.0);
    {
        double acc = 0.0;
        for (int i = 1; i < probSamples.size(); ++i) {
            acc += distance(probSamples[i - 1].position,
                            probSamples[i].position);
            sampleTravel[i] = acc;
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
    // m_pendingDirtyRow may have referenced the now-gone row.
    m_pendingDirtyRow = -1;

    int candidateIdx = candidateIdxBefore;
    if (probationIdx < candidateIdxBefore) {
        candidateIdx -= 1;
    }
    if (candidateIdx < 0 || candidateIdx >= m_strokes.size()) {
        // Safety net: candidate vanished between arm and commit.
        m_continuationState = ContinuationState();
        emit continuationRejected();
        return;
    }

    cwPenStroke &cand = m_strokes[candidateIdx];

    // Keep the pre-overlap prefix of the (possibly reversed) candidate. In
    // backward mode the first-hit segment's upper reversed-frame index is
    // N − firstHitSeg, so that many leading points survive.
    int prefixCount;
    if (backward) {
        std::reverse(cand.points.begin(), cand.points.end());
        prefixCount = cand.points.size() - firstHitSeg;
    } else {
        prefixCount = firstHitSeg;
    }
    prefixCount = std::clamp<int>(prefixCount, 0, cand.points.size());

    QVector<cwPenPoint> newPoints;
    newPoints.reserve(prefixCount + probSamples.size());
    for (int i = 0; i < prefixCount; ++i) {
        newPoints.append(cand.points[i]);
    }

    // t=0 stored sits on the old line (smoothing early jitter); t=1 equals
    // raw pen so post-commit raw appends continue seamlessly.
    for (int i = 0; i < probSamples.size(); ++i) {
        const double t = (window > 0.0)
            ? std::clamp(sampleTravel[i] / window, 0.0, 1.0)
            : 1.0;
        cwPenPoint bp = probSamples[i];
        bp.position = blendSample(overlapStart, overlapEnd,
                                  probSamples[i].position, t);
        newPoints.append(bp);
    }
    cand.points = newPoints;

    // Structural change to the polyline isn't reliably consumed via
    // dataChanged alone, so reset the model around the replacement.
    applyStrokes(m_strokes);

    // `used` survives reset-and-set so endStroke labels the undo entry
    // "Continue Stroke". Subsequent appendPoint calls store raw.
    m_continuationState = ContinuationState();
    m_continuationState.used = true;
    m_continuationState.strokeIndex = candidateIdx;

    emit continuationCommitted(candidateIdx);
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
    const QString &brushName,
    QPointF worldPoint,
    double probationWindowScreenPx) const
{
    cwSketchContinuationTarget result;
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
           "findContinuationTarget: brush=%s pen=(%.4f, %.4f) "
           "paperPpm=%.4f pxPerMeter=%.4f zoom=%.4f strokes=%d",
           qUtf8Printable(brushName), worldPoint.x(), worldPoint.y(),
           paperPpm, pxPerMeter,
           m_viewState->zoom(),
           static_cast<int>(m_strokes.size()));

    double bestDistSq = std::numeric_limits<double>::infinity();
    int     bestSegIdx = -1;
    QPointF bestProj;
    QPointF bestTangent;

    for (int sIdx = 0; sIdx < m_strokes.size(); ++sIdx) {
        const cwPenStroke &stroke = m_strokes[sIdx];
        if (stroke.brushName != brushName) {
            continue;
        }
        if (stroke.points.size() < 2) {
            continue;
        }

        // Threshold = half the *rendered* world width = 0.5 ×
        // kSketchStrokeRenderWidth / paperPpm. Matches the outer edge of the
        // visible stroke. Use 0.25 for inner-half only if edge-grazes feel wrong.
        const double thresholdMeters = 0.5 * kSketchStrokeRenderWidth / paperPpm;
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
            result.strokeWidth = kSketchStrokeRenderWidth;
            bestSegIdx = i;
            // Re-project to compute proj + tangent for this winning segment.
            const QPointF ab = b - a;
            const double lenSq = ab.x() * ab.x() + ab.y() * ab.y();
            if (lenSq > 0.0) {
                const QPointF ap = worldPoint - a;
                const double t = std::clamp(
                    (ap.x() * ab.x() + ap.y() * ab.y()) / lenSq, 0.0, 1.0);
                bestProj = a + t * ab;
                const double len = std::sqrt(lenSq);
                bestTangent = QPointF(ab.x() / len, ab.y() / len);
            } else {
                bestProj = a;
                bestTangent = QPointF(1.0, 0.0);
            }
        }
        const double minDist = std::sqrt(strokeBestDistSq);
        qCInfo(lcContinuation,
               "  stroke[%d] brush=%s width=%.4f thresholdWorld=%.5f "
               "thresholdPaperPx=%.4f thresholdScreenPx=%.4f "
               "minDistWorld=%.5f minDistPaperPx=%.4f minDistScreenPx=%.4f %s",
               sIdx, qUtf8Printable(stroke.brushName), kSketchStrokeRenderWidth,
               thresholdMeters, thresholdPaperPx, thresholdMeters * pxPerMeter,
               minDist, minDist * paperPpm, minDist * pxPerMeter,
               (strokeBestDistSq <= thresholdSq ? "HIT" : "miss"));
    }

    // Endpoint arclength check: if the landing projection is within one
    // probation window of an endpoint, promote the target to the fast path.
    // Walk outward from the hit segment in each direction, early-outing when
    // the accumulator passes the threshold — cost is O(segments-within-one-
    // probation-window), not O(N).
    if (result.strokeIndex >= 0 && probationWindowScreenPx > 0.0) {
        const double probationWindowMeters =
            probationWindowScreenPx / pxPerMeter;
        const cwPenStroke &stroke = m_strokes[result.strokeIndex];
        const int N = stroke.points.size();

        double forwardArc = distance(bestProj, stroke.points[bestSegIdx].position);
        for (int j = bestSegIdx;
             j < N - 1 && forwardArc < probationWindowMeters; ++j) {
            forwardArc += distance(stroke.points[j].position,
                                   stroke.points[j + 1].position);
        }

        double backwardArc =
            distance(bestProj, stroke.points[bestSegIdx - 1].position);
        for (int j = bestSegIdx - 1;
             j > 0 && backwardArc < probationWindowMeters; --j) {
            backwardArc += distance(stroke.points[j].position,
                                    stroke.points[j - 1].position);
        }

        const bool reachedTail = forwardArc < probationWindowMeters;
        const bool reachedHead = backwardArc < probationWindowMeters;

        using Endpoint = cwSketchContinuationTarget::Endpoint;
        if (reachedHead && reachedTail) {
            // Two-point stroke or very short one within both arclengths:
            // pick the nearer; tie favors Tail (plan edge case).
            result.endpoint = (backwardArc < forwardArc) ? Endpoint::Head
                                                         : Endpoint::Tail;
        } else if (reachedTail) {
            result.endpoint = Endpoint::Tail;
        } else if (reachedHead) {
            result.endpoint = Endpoint::Head;
        }

        if (result.endpoint != Endpoint::None) {
            result.hitSegment = bestSegIdx;
            result.hitWorld = bestProj;
            result.hitTangent = bestTangent;
        }

        qCInfo(lcContinuation,
               "  endpoint-scan: forwardArc=%.5f backwardArc=%.5f "
               "window=%.5f → endpoint=%d hitSegment=%d",
               forwardArc, backwardArc, probationWindowMeters,
               static_cast<int>(result.endpoint), result.hitSegment);
    }

    qCInfo(lcContinuation, "findContinuationTarget → strokeIndex=%d strokeWidth=%.4f endpoint=%d",
           result.strokeIndex, result.strokeWidth,
           static_cast<int>(result.endpoint));
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

    // Cache the candidate's threshold-expanded bbox for the per-sample
    // AABB early-reject in handleProbationSample.
    const double thr = m_continuationState.hitThresholdMeters;
    m_continuationState.candidateBbox = m_strokes[candidate.strokeIndex]
        .boundingBox()
        .adjusted(-thr, -thr, thr, thr);

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

void cwSketch::commitAtEndpoint(int probationStrokeIndex,
                                cwSketchContinuationTarget candidate)
{
    using Endpoint = cwSketchContinuationTarget::Endpoint;
    if (probationStrokeIndex < 0 || probationStrokeIndex >= m_strokes.size()) {
        return;
    }
    if (candidate.strokeIndex < 0 || candidate.strokeIndex >= m_strokes.size()) {
        return;
    }
    if (candidate.strokeIndex == probationStrokeIndex) {
        return;
    }
    if (candidate.endpoint == Endpoint::None) {
        return;
    }
    if (candidate.hitSegment <= 0) {
        return;
    }

    const bool backward = (candidate.endpoint == Endpoint::Head);
    const int probationIdx = probationStrokeIndex;
    int candidateIdx = candidate.strokeIndex;
    const int hitSeg = candidate.hitSegment;

    // Blend window = centerline arclength from the landing projection to
    // the original endpoint; cached before the reverse + insert so the
    // EndpointBlend phase can smooth the user's overdraw across the
    // remaining tip instead of discarding it.
    const cwPenStroke &origCand = m_strokes[candidateIdx];
    const int Norig = origCand.points.size();
    QPointF endpointBlendEndWorld;
    double  endpointBlendWindowMeters = 0.0;
    if (hitSeg > 0 && hitSeg <= Norig - 1) {
        if (backward) {
            endpointBlendEndWorld = origCand.points.first().position;
            endpointBlendWindowMeters =
                distance(candidate.hitWorld, origCand.points[hitSeg - 1].position);
            for (int j = hitSeg - 1; j > 0; --j) {
                endpointBlendWindowMeters += distance(
                    origCand.points[j].position,
                    origCand.points[j - 1].position);
            }
        } else {
            endpointBlendEndWorld = origCand.points.last().position;
            endpointBlendWindowMeters =
                distance(candidate.hitWorld, origCand.points[hitSeg].position);
            for (int j = hitSeg; j < Norig - 1; ++j) {
                endpointBlendWindowMeters += distance(
                    origCand.points[j].position,
                    origCand.points[j + 1].position);
            }
        }
    }

    qCInfo(lcContinuation,
           "commitAtEndpoint: probationRow=%d candidateRow=%d endpoint=%s "
           "hitSeg=%d hitWorld=(%.4f, %.4f) blendEnd=(%.4f, %.4f) "
           "blendWindow=%.5f m origCandPoints=%d",
           probationIdx, candidateIdx,
           backward ? "Head" : "Tail",
           hitSeg,
           candidate.hitWorld.x(), candidate.hitWorld.y(),
           endpointBlendEndWorld.x(), endpointBlendEndWorld.y(),
           endpointBlendWindowMeters,
           Norig);

    m_strokeModel->beginRemoveRows(QModelIndex(), probationIdx, probationIdx);
    m_strokes.removeAt(probationIdx);
    m_strokeModel->endRemoveRows();
    m_pendingDirtyRow = -1;

    if (probationIdx < candidateIdx) {
        candidateIdx -= 1;
    }
    if (candidateIdx < 0 || candidateIdx >= m_strokes.size()) {
        return;
    }

    cwPenStroke &cand = m_strokes[candidateIdx];
    const int N = cand.points.size();
    if (hitSeg <= 0 || hitSeg > N - 1) {
        return;
    }

    int prefixCount;
    if (backward) {
        std::reverse(cand.points.begin(), cand.points.end());
        // Original segment (hitSeg-1, hitSeg) maps to reversed indices
        // (N-1-hitSeg, N-hitSeg), so that many leading points survive.
        prefixCount = N - hitSeg;
    } else {
        prefixCount = hitSeg;
    }
    prefixCount = std::clamp(prefixCount, 0, N);

    // Insert hitWorld without truncating so the old tail stays visible;
    // the blend phase removes old-tail vertices one at a time as the pen
    // arc passes each.
    cand.points.insert(prefixCount, cwPenPoint(candidate.hitWorld, 1.0, 0));
    const int oldTailHeadIdx = prefixCount + 1;

    // Walk forward through the array so this works identically for Tail
    // commits and post-reverse Head commits.
    QVector<double> oldTailArcs;
    oldTailArcs.reserve(cand.points.size() - oldTailHeadIdx);
    {
        double acc = 0.0;
        QPointF prev = candidate.hitWorld;
        for (int j = oldTailHeadIdx; j < cand.points.size(); ++j) {
            const QPointF q = cand.points[j].position;
            acc += distance(prev, q);
            oldTailArcs.append(acc);
            prev = q;
        }
    }

    // Structural change (reverse + insert); reset the model so the batching
    // painter path picks up the new geometry cleanly.
    applyStrokes(m_strokes);

    m_continuationState = ContinuationState();
    m_continuationState.used = true;
    m_continuationState.strokeIndex = candidateIdx;
    if (endpointBlendWindowMeters > 0.0) {
        m_continuationState.phase = ContinuationState::Phase::EndpointBlend;
        m_continuationState.endpointBlendStartWorld = candidate.hitWorld;
        m_continuationState.endpointBlendEndWorld = endpointBlendEndWorld;
        m_continuationState.endpointBlendWindowMeters =
            endpointBlendWindowMeters;
        m_continuationState.endpointOldTailArcs = oldTailArcs;
        m_continuationState.endpointOldTailHeadIdx = oldTailHeadIdx;
    }

    emit continuationCommitted(candidateIdx);
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
