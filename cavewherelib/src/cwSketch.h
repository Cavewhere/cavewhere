/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCH_H
#define CWSKETCH_H

//Qt includes
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QUndoStack>
#include <QUuid>
#include <QVector>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwPenStroke.h"
#include "cwSketchData.h"
#include "cwSketchViewState.h"
#include "cwSurvey2DGeometryArtifact.h"
#include "cwSurveyNetworkArtifact.h"

class cwScale;
class cwKeywordModel;
class cwPenStrokeModel;
class cwAbstractScrapViewMatrix;
class cwMatrix4x4Artifact;
class cwSurvey2DGeometryRule;
class cwTrip;

// Result of cwSketch::findContinuationTarget. strokeIndex == -1 means no
// qualifying stroke was within proximity. armProbation() reads strokeWidth to
// derive the per-sample hit-test radius (0.5 × strokeWidth screen pixels).
class CAVEWHERE_LIB_EXPORT cwSketchContinuationTarget {
    Q_GADGET
    QML_VALUE_TYPE(sketchContinuationTarget)
    Q_PROPERTY(int strokeIndex MEMBER strokeIndex)
    Q_PROPERTY(double strokeWidth MEMBER strokeWidth)

public:
    int    strokeIndex = -1;
    double strokeWidth = 0.0;
};

Q_DECLARE_METATYPE(cwSketchContinuationTarget)

class CAVEWHERE_LIB_EXPORT cwSketch : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sketch)

    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(ViewType viewType READ viewType WRITE setViewType NOTIFY viewTypeChanged)
    Q_PROPERTY(cwScale* mapScale READ mapScale CONSTANT)
    Q_PROPERTY(QString iconImagePath READ iconImagePath WRITE setIconImagePath NOTIFY iconImagePathChanged)
    Q_PROPERTY(cwPenStrokeModel* strokeModel READ strokeModel CONSTANT)
    Q_PROPERTY(QUndoStack* undoStack READ undoStack CONSTANT)
    Q_PROPERTY(cwKeywordModel* keywordModel READ keywordModel CONSTANT)
    Q_PROPERTY(cwSurveyNetworkArtifact* surveyNetworkArtifact READ surveyNetworkArtifact WRITE setSurveyNetworkArtifact NOTIFY surveyNetworkArtifactChanged)
    Q_PROPERTY(cwSurvey2DGeometryArtifact* survey2DGeometry READ survey2DGeometry CONSTANT)
    Q_PROPERTY(QString anchorStation READ anchorStation WRITE setAnchorStation NOTIFY anchorStationChanged)
    Q_PROPERTY(cwSketchViewState* viewState READ viewState CONSTANT)

public:
    enum ViewType {
        Plan = 0,
        RunningProfile = 1,
        ProjectedProfile = 2
    };
    Q_ENUM(ViewType)

    explicit cwSketch(QObject *parent = nullptr);
    ~cwSketch() override;

    QString name() const { return m_name; }
    void setName(const QString &name);

    QUuid id() const { return m_id; }
    void setId(const QUuid &id);

    ViewType viewType() const { return m_viewType; }
    void setViewType(ViewType type);

    cwScale *mapScale() const { return m_mapScale; }

    // Icon thumbnails are written by cwSketchManager into the shared project
    // disk cache and exposed here as an `image://cwcache/...` URL. The sketch
    // itself does not know how to produce or resolve them.
    QString iconImagePath() const { return m_iconImagePath; }
    void setIconImagePath(const QString &path);

    // Station within parentTrip() that selects *which* connected component of
    // the trip's line plot this sketch renders, AND whose world position is
    // translated to the sketch's (0,0). Empty until the user picks it (or the
    // single-component auto-pick fires). Persisted across loads.
    QString anchorStation() const { return m_anchorStation; }
    void setAnchorStation(const QString &name);

    cwSketchViewState *viewState() const { return m_viewState; }

    const QVector<cwPenStroke> &strokes() const { return m_strokes; }
    void setStrokes(const QVector<cwPenStroke> &strokes);

    cwPenStrokeModel *strokeModel() const { return m_strokeModel; }
    QUndoStack       *undoStack() const { return m_undoStack; }
    cwKeywordModel   *keywordModel() const { return m_keywordModel; }

    // Region-wide network input; typically pointed at
    // cwLinePlotManager::surveyNetworkArtifact(). When it or the view matrix
    // changes, survey2DGeometry() regenerates.
    cwSurveyNetworkArtifact   *surveyNetworkArtifact() const;
    void setSurveyNetworkArtifact(cwSurveyNetworkArtifact *artifact);

    cwSurvey2DGeometryArtifact *survey2DGeometry() const;

    void setParentTrip(cwTrip *trip);
    Q_INVOKABLE cwTrip *parentTrip() const { return m_parentTrip; }

    Q_INVOKABLE int  beginStroke(cwPenStroke::Kind kind, double width, const QColor &color = QColor());

    // Scans same-`kind` strokes (Eraser is always skipped) for one whose
    // centerline is within 0.5×stroke.width *screen pixels* (converted to
    // world meters via viewState->pixelsPerMeter()) of `worldPoint`. Returns
    // the nearest qualifying stroke. When the matrix is unset the scan is
    // skipped and strokeIndex=-1 is returned.
    Q_INVOKABLE cwSketchContinuationTarget findContinuationTarget(
        cwPenStroke::Kind kind, QPointF worldPoint) const;

    // Arms a probation window on the active stroke (created by QML via
    // beginStroke; its row index is `probationStrokeIndex`). Subsequent
    // appendPoint calls on that row hit-test against `candidate`'s
    // centerline and are buffered on the probation row. After
    // `probationWindowScreenPx` of accumulated raw pen travel, if
    // hitCount/sampleCount >= commitHitRateThreshold the probation row is
    // removed, the candidate's points in the overlap region are replaced
    // with a sequence of blended samples (lerp between the old centerline
    // and the raw pen, with weight swept 0→1 across the probation window
    // so the seam smooths the user's overdraw into the existing line), and
    // `continuationCommitted` fires with the candidate's new row index.
    // Otherwise `continuationRejected` fires and the probation row is left
    // as a normal fresh stroke. After commit, further samples append raw.
    Q_INVOKABLE void armProbation(int probationStrokeIndex,
                                  cwSketchContinuationTarget candidate,
                                  double probationWindowScreenPx,
                                  double commitHitRateThreshold = 0.6);

    // Contract: appendPoint is for the live pen input stream only — one
    // active row at a time, between beginStroke() and endStroke(). The
    // per-frame coalescer uses a single-row sentinel, so calling this across
    // different strokeIndex values in one event-loop iteration would drop
    // dataChanged() emits on all but the first row. Bulk programmatic inserts
    // (point symbols, fill areas, etc.) should go through a future bulk API
    // that batches its own begin/endInsertRows, not through appendPoint.
    Q_INVOKABLE void appendPoint(int strokeIndex, const cwPenPoint &p);
    Q_INVOKABLE void appendPoint(int strokeIndex, QPointF position, double pressure, qint64 timestampMs = 0);
    Q_INVOKABLE void endStroke();
    Q_INVOKABLE void clearStrokes();

    // Splits any stored stroke whose points fall within radiusWorld of the
    // eraser polyline. Surviving point runs become new strokes (fresh id,
    // same kind/width/color). Per-pointer-move calls within one drag merge
    // into a single undo step; endEraseSession() closes that window.
    Q_INVOKABLE void eraseAlongPath(const QVector<QPointF> &pathPointsWorld,
                                    double radiusWorld);

    // Closes the current erase merge window so the next eraseAlongPath call
    // begins a fresh undo entry. Called on pen-up.
    Q_INVOKABLE void endEraseSession();

    cwSketchData data() const;
    void setData(const cwSketchData &d);

signals:
    void nameChanged();
    void viewTypeChanged();
    void iconImagePathChanged();
    void strokesReset();
    void strokeEnded();
    // Toggles around the live drawing window (begin/endStroke). cwSketchManager
    // uses this to suppress async icon rendering while a stroke is in flight.
    void activeDrawingChanged(bool active);
    void surveyNetworkArtifactChanged();
    void anchorStationChanged();

    // Fired from inside appendPoint() when probation passes the hit-rate
    // threshold. The probation row has been removed from m_strokes and the
    // candidate (now `newActiveStrokeIndex`) has been truncated + grafted.
    // QML must update its `_activeStrokeIndex` to keep subsequent
    // appendPoint calls targeting the right row.
    void continuationCommitted(int newActiveStrokeIndex);
    // Fired from inside appendPoint() when probation fails. The probation
    // row stays as a normal fresh stroke; QML keeps its existing index.
    void continuationRejected();

private:
    QVector<cwPenStroke> m_strokes;
    QVector<cwPenStroke> m_startStrokes;

    QString  m_name;
    QUuid    m_id;
    ViewType m_viewType = Plan;
    cwScale *m_mapScale = nullptr;
    QString  m_iconImagePath;
    QString  m_anchorStation;

    cwPenStrokeModel  *m_strokeModel = nullptr;
    QUndoStack        *m_undoStack   = nullptr;
    cwKeywordModel    *m_keywordModel = nullptr;
    cwSketchViewState *m_viewState   = nullptr;

    cwAbstractScrapViewMatrix *m_viewMatrix      = nullptr;
    cwMatrix4x4Artifact       *m_matrixArtifact  = nullptr;
    cwSurvey2DGeometryRule    *m_geometryRule    = nullptr;
    cwTrip                    *m_parentTrip      = nullptr;

    // -1 sentinel doubles as the "no flush scheduled" flag; see Decision 2
    // in the sketch feature plan for why coalescing matters.
    int m_pendingDirtyRow = -1;

    // Monotonic id that identifies one pen drag's worth of erases. Each
    // cwSketchEraseCommand captures this value at construction; merges are
    // allowed only between commands that share an id, so endEraseStroke()
    // bumps it to start a fresh undo entry for the next drag.
    int m_eraseSession = 0;

    // Two-phase state machine driven by appendPoint().
    //
    //   Off       : normal stroke; appendPoint stores raw.
    //   Probation : armed by armProbation(). Each sample hit-tests against
    //               candidate centerline; we buffer raw samples on the
    //               probation row and accumulate pen travel until it
    //               exceeds probationWindowMeters, then commit or reject.
    //               On commit the buffered probation samples are replayed
    //               as blended points into the candidate (lerping with the
    //               old centerline in the overlap region), and the state
    //               returns to Off — no separate post-commit blend phase.
    //
    // `used` stays true once probation commits, so endStroke() picks the
    // "Continue Stroke" undo label.
    struct ContinuationState {
        enum class Phase { Off, Probation };
        Phase   phase = Phase::Off;
        bool    used = false;
        int     strokeIndex = -1;     // The row appendPoint is targeting.

        // Probation bookkeeping.
        int     candidateIndex = -1;
        double  hitThresholdMeters = 0.0;     // 0.5 × strokeWidthScreenPx / pxPerMeter.
        double  probationWindowMeters = 0.0;
        double  commitHitRateThreshold = 0.6;
        int     hitCount = 0;
        int     sampleCount = 0;

        // Direction resolution.
        //
        // Probation tracks both the highest-index (forward) and lowest-index
        // (backward) in-proximity hit so commit can truncate from either end.
        // The direction itself is decided at commit time by projecting the
        // pen's net motion (lastRaw − probationStart) onto the candidate
        // tangent sampled at the *first* hit — a positive dot product means
        // the user is extending forward (keep current behavior), negative
        // means they're extending backward (reverse the candidate so its
        // head becomes its tail, then truncate normally).
        QPointF probationStartWorld;           // First raw pen position.
        bool    haveProbationStart = false;
        int     firstHitSegmentIndex = -1;     // Segment of the first hit.
        QPointF firstHitWorld;                 // Projected first-hit world position. Anchors the overlap's start.
        QPointF firstHitTangent;               // Forward tangent at the first hit.
        int     furthestForwardSeg = -1;       // Max-index hit.
        QPointF furthestForwardWorld;
        QPointF furthestForwardTangent;
        int     furthestBackwardSeg = -1;      // Min-index hit.
        QPointF furthestBackwardWorld;
        QPointF furthestBackwardTangent;

        // Raw-travel tracker (used to decide when the window closes and to
        // compute per-sample blend weights at commit time).
        double  travelMeters = 0.0;            // Cumulative raw arclength since arming.
        QPointF lastRawWorld;
        bool    haveLastRawWorld = false;
    };
    ContinuationState m_continuationState;

    void applyStrokes(const QVector<cwPenStroke> &strokes);
    void rebuildViewMatrixForType();
    void syncMatrixArtifact();
    // Coalesces per-sample dataChanged emits into one queued call per row so
    // a fast pen drag doesn't flood Qt's event loop. Used by appendPoint
    // (both the probation and the post-probation paths).
    void scheduleDirtyEmit(int row);
};

#endif // CWSKETCH_H
