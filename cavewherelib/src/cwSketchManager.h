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
#include <QSet>
#include <QTimer>
#include <memory>
#include <unordered_map>

//Our includes
#include "CaveWhereLibExport.h"
#include "cwDiskCacher.h"
#include "cwTripLinePlotTask.h"
#include "cwUniqueConnectionChecker.h"
#include "asyncfuture.h"

class cwProject;
class cwRegionTreeModel;
class cwTrip;
class cwSketch;
class cwSketchCanvas;
class cwSurveyNoteSketchModel;
class cwSurveyChunk;

/**
 * @brief Rasterises sketch thumbnails into the shared project disk cache and
 * exposes them via the `image://cwcache/...` provider URL.
 *
 * Mirrors cwNoteLiDARManager's lifecycle: walks the region tree to discover
 * trips, subscribes to each trip's sketch model, and tracks per-sketch
 * mutations (strokes reset / stroke-model rows inserted/removed/changed).
 * Changes coalesce through a single-shot timer so a burst of stroke appends
 * produces one write.
 */
class CAVEWHERE_LIB_EXPORT cwSketchManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(SketchManager)

public:
    explicit cwSketchManager(QObject* parent = nullptr);
    ~cwSketchManager() override;

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);

    // Forces a rasterise + cache write for a single sketch. Primarily for
    // tests; production writes happen via the dirty-batch timer.
    Q_INVOKABLE void rasteriseNow(cwSketch* sketch);

    static cwDiskCacher::Key cacheKey(const cwSketch* sketch);
    // `version` is appended as a `?v=<version>` query so QML's Image cache
    // invalidates whenever the icon changes. Pass an empty string to omit.
    static QString cacheUrl(const cwDiskCacher::Key& key, const QString& version);

    // Renders the sketch's strokes into a square QImage of the given edge size.
    static QImage renderIcon(const cwSketch* sketch, int edgePixels = 256);

    // -------------------- Per-trip line plot pipeline --------------------
    //
    // Sketches share one cwTripLinePlotTask pipeline per cwTrip. A canvas
    // acquires the pipeline when it attaches to a sketch, and releases it
    // when the sketch changes or the canvas is destroyed. The pipeline is
    // created lazily on first acquire and destroyed when the refcount drops
    // to zero — no background warming.
    //
    // Updates are coalesced through AsyncFuture::Restarter: a burst of
    // survey-editor keystrokes yields one cavern run per settle.

    // Registers `canvas` as a consumer of `trip`'s line plot. Safe with
    // null arguments (no-op). Triggers a first run if this is the initial
    // consumer; returns immediately either way.
    void acquireLinePlot(cwSketchCanvas* canvas, cwTrip* trip);

    // Removes `canvas` from `trip`'s consumer list. On reaching zero
    // consumers, disconnects signals and drops the cached components.
    void releaseLinePlot(cwSketchCanvas* canvas, cwTrip* trip);

    // Most recent settled components for `trip`. Empty while the first run
    // is in flight, when the trip has no chunks, or when no consumer is
    // currently attached.
    QList<cwTripLinePlotTask::TripComponent> latestLinePlot(cwTrip* trip) const;

signals:
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
    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    void connectSketch(cwSketch* sketch);
    void disconnectSketch(cwSketch* sketch);

    void updateIconFromCache(cwSketch* sketch);

    void markDirty(cwSketch* sketch);
    void flushDirty();

    void writeIcon(cwSketch* sketch);

    QDir cacheDir() const;

    // ---- Per-trip line plot pipeline (see header comment above) ----

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

    QSet<cwSketch*> m_dirtySketches;
    QTimer m_flushTimer;

    cwUniqueConnectionChecker m_connectionChecker;

    std::unordered_map<cwTrip*, std::unique_ptr<TripPipeline>> m_tripPipelines;
};

#endif // CWSKETCHMANAGER_H
