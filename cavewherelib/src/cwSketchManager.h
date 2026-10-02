/**************************************************************************
**
**    Copyright (C) 2026 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSKETCHMANAGER_H
#define CWSKETCHMANAGER_H

//Qt includes
#include <QHash>
#include <QImage>
#include <QList>
#include <QMetaObject>
#include <QModelIndex>
#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QRectF>
#include <QSet>
#include <QTimer>
#include <atomic>
#include <memory>
#include <unordered_map>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "cwPenStroke.h"
#include "cwTripLinePlotTask.h"
#include "cwConnectionRegistry.h"
#include "asyncfuture.h"

class cwProject;
class cwRegionTreeModel;
class cwTrip;
class cwSketch;
class cwSurveyNoteSketchModel;
class cwSurveyChunk;

/**
 * @brief Rasterises sketch thumbnails into the shared project disk cache and
 * exposes them via the `image://cwcache/...` provider URL.
 *
 * Renders run asynchronously on cwConcurrent's worker pool, coalesced through
 * AsyncFuture::Restarter. Two modes govern when a render starts:
 *
 *  - `autoIconUpdates = true`:  after a pen-up, wait `idleIntervalMs` of no
 *    further edits, then submit a render. Any stroke begin cancels the
 *    in-flight render before the disk write lands.
 *  - `autoIconUpdates = false`: edits only bump a dirty epoch; renders happen
 *    solely via explicit `flushIconIfDirty()` calls from the UI (typically on
 *    navigation away from or onto a sketch).
 *
 * Mirrors cwNoteLiDARManager's lifecycle: walks the region tree to discover
 * trips, subscribes to each trip's sketch model, and tracks per-sketch
 * mutations.
 */
class CAVEWHERE_LIB_EXPORT cwSketchManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchManager)

    Q_PROPERTY(bool autoIconUpdates READ autoIconUpdates WRITE setAutoIconUpdates NOTIFY autoIconUpdatesChanged)
    Q_PROPERTY(int  idleIntervalMs  READ idleIntervalMs  WRITE setIdleIntervalMs  NOTIFY idleIntervalMsChanged)

public:
    explicit cwSketchManager(QObject* parent = nullptr);
    ~cwSketchManager() override;

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);

    bool autoIconUpdates() const { return m_autoIconUpdates; }
    void setAutoIconUpdates(bool enabled);

    int  idleIntervalMs() const { return m_idleIntervalMs; }
    void setIdleIntervalMs(int ms);

    // Forces a synchronous rasterise + cache write. Primarily for tests;
    // production writes happen via the async pipeline.
    Q_INVOKABLE void rasteriseNow(cwSketch* sketch);

    // Async refresh — if the sketch has unflushed edits and is not actively
    // being drawn, submits a render now (bypassing the idle debounce). No-op
    // otherwise. Used by navigation events (sketch deselection, gallery taps).
    // Accepts a generic QObject* so QML callers can pass a note model entry
    // without type-checking; non-sketch objects are silently ignored.
    Q_INVOKABLE void flushIconIfDirty(QObject* sketchObject);

    static cwDiskCacher::Key cacheKey(const cwSketch* sketch);
    // `version` is appended as a `?v=<version>` query so QML's Image cache
    // invalidates whenever the icon changes. Pass an empty string to omit.
    static QString cacheUrl(const cwDiskCacher::Key& key, const QString& version);

    // Renders the sketch's strokes into a square QImage of the given edge size.
    static QImage renderIcon(const cwSketch* sketch, int edgePixels = 256);

    // Worker-safe variant. No QObject dependencies — callable from any thread
    // once the caller has snapshotted the strokes on the GUI thread.
    static QImage renderIconFromSnapshot(const QVector<cwPenStroke>& strokes,
                                         int edgePixels = 256);

    // -------------------- Per-trip line plot pipeline --------------------
    //
    // Sketches share one cwTripLinePlotTask pipeline per cwTrip. A canvas
    // acquires the pipeline when it attaches to a sketch, and releases it
    // when the sketch changes or the canvas is destroyed. The pipeline is
    // created lazily on first acquire and destroyed when the refcount drops
    // to zero — no background warming.

    void acquireLinePlot(cwTrip* trip);
    void releaseLinePlot(cwTrip* trip);
    QList<cwTripLinePlotTask::TripComponent> latestLinePlot(cwTrip* trip) const;

    // Per-sketch registration. Normally invoked automatically via the region
    // tree / trip wiring; exposed so unit tests can wire a bare sketch.
    void attachSketch(cwSketch* sketch);
    void detachSketch(cwSketch* sketch);

signals:
    void autoIconUpdatesChanged();
    void idleIntervalMsChanged();

    // Emitted on the GUI thread after a pipeline run completes for `trip`.
    // Consumers should re-query latestLinePlot(trip) on this signal.
    void linePlotUpdated(cwTrip* trip);

private slots:
    void handleRegionReset();
    void regionRowsInserted(const QModelIndex& parent, int begin, int end);
    void regionRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end);

    void sketchRowsInserted(const QModelIndex& parent, int begin, int end);
    void sketchRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end);

    void sketchDestroyed(QObject* sketchObj);

private:
    // Per-sketch async pipeline. Created lazily in connectSketch(); torn down
    // in disconnectSketch() / sketchDestroyed(). The Restarter coalesces rapid
    // submits into a single in-flight render, and cancels the in-flight future
    // (propagating to the worker via QPromise::isCanceled) when a new one
    // arrives or when the user resumes drawing.
    using IconRestarter = AsyncFuture::Restarter<QByteArray>;

    struct SketchPipeline {
        std::unique_ptr<IconRestarter> restarter;
        std::unique_ptr<QTimer>        idleTimer;
        quint64 dirtyEpoch     = 0;
        quint64 submittedEpoch = 0;
        quint64 completedEpoch = 0;
        bool    activeDrawing  = false;
        // Flipped to true to drop the in-flight render before its disk write
        // lands (cancel-on-resume). The worker polls this between expensive
        // steps. A fresh token is allocated on every submitRender() so a
        // cancellation never carries over to a subsequent run.
        std::shared_ptr<std::atomic<bool>> cancelToken;
    };

    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    void connectSketch(cwSketch* sketch);
    void wireSketch(cwSketch* sketch);
    void disconnectSketch(cwSketch* sketch);

    void updateIconFromCache(cwSketch* sketch);

    SketchPipeline* pipelineFor(cwSketch* sketch);
    void markDirty(cwSketch* sketch);
    void onActiveDrawingChanged(cwSketch* sketch, bool active);
    void onIdleTimeout(cwSketch* sketch);
    void submitRender(cwSketch* sketch);

    // Synchronous path used by rasteriseNow() and legacy tests. Not reached by
    // the async pipeline.
    void writeIconSync(cwSketch* sketch);

    QDir cacheDir() const;

    // ---- Per-trip line plot pipeline ----

    using Restarter = AsyncFuture::Restarter<QList<cwTripLinePlotTask::TripComponent>>;

    struct TripPipeline {
        std::unique_ptr<Restarter> restarter;
        QList<cwTripLinePlotTask::TripComponent> latest;
        int refCount = 0;
        QList<QMetaObject::Connection> tripConnections;
        QList<QMetaObject::Connection> chunkConnections;
        QMetaObject::Connection destroyedConnection;
    };

    void connectLinePlotTripSignals(cwTrip* trip, TripPipeline& pipeline);
    void connectLinePlotChunkSignals(cwTrip* trip, TripPipeline& pipeline);
    void disconnectLinePlotSignals(TripPipeline& pipeline);
    void rebuildChunkSignals(cwTrip* trip);
    void requestLinePlotRerun(cwTrip* trip);
    void onTripDestroyed(cwTrip* trip);

    QPointer<cwRegionTreeModel> m_regionModel;
    QPointer<cwProject> m_project;

    std::unordered_map<cwSketch*, std::unique_ptr<SketchPipeline>> m_sketchPipelines;

    bool m_autoIconUpdates = true;
    int  m_idleIntervalMs  = 3000;

    cwConnectionRegistry m_connectionRegistry;

    std::unordered_map<cwTrip*, std::unique_ptr<TripPipeline>> m_tripPipelines;
};

#endif // CWSKETCHMANAGER_H
