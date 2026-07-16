/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPMANAGER_H
#define CWSCRAPMANAGER_H

//Qt includes
#include "cwRenderTexturedItems.h"
#include <QObject>
#include <QModelIndex>
#include <QSet>
#include <QHash>
#include <QUuid>
#include <QWeakPointer>
#include <QPointer>
#include <QMetaObject>
#include <QQmlEngine>
#include <QFuture>
#include <QPolygonF>
#include <QVector>
#include <QPair>

//Our includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwScrap;
class cwNote;
class cwSketch;
class cwTriangulateTask;
class cwProject;
class cwGLScraps;
class cwStationPositionLookup;
class cwRemoveImageTask;
class cwLinePlotManager;
class cwTaskManagerModel;
class cwRegionTreeModel;
class cwRenderScraps;
class cwKeywordItemModel;
class cwSketchManager;
#include "cwNoteStation.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImageProvider.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "asyncfuture.h"
#include "cwTriangulateWarping.h"
#include "cwSketchScrapOutline.h"
#include "cwKeywordItemRegistry.h"
#include <memory>

/**
    The scrap manager listens to changes in the notes and creates all
    the geometry need to show a scrap in 3d
  */
class CAVEWHERE_LIB_EXPORT cwScrapManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScrapManager)

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)
    Q_PROPERTY(cwTriangulateWarping* warpingSettings READ warpingSettings CONSTANT)

public:
    explicit cwScrapManager(QObject *parent = 0);
    ~cwScrapManager();

    struct TriangulatedScrapResult {
        cwScrap* scrap = nullptr;
        QFuture<cwTriangulatedData> data;
    };

    // Snapshot of data the sketch-scrap derivation pipeline produced for one
    // outline — exposed so the sketch view can overlay it for debugging.
    struct SketchScrapDebugStation {
        QString name;
        QPointF tripLocalPos;
        bool operator==(const SketchScrapDebugStation&) const = default;
    };

    struct SketchScrapDebugEntry {
        QVector<QUuid> memberStrokeIds;
        QPolygonF tripLocalPolygon;
        QVector<SketchScrapDebugStation> stations;
        bool operator==(const SketchScrapDebugEntry&) const = default;
    };

    const QVector<SketchScrapDebugEntry>& sketchDebugEntries(cwSketch* sketch) const {
        static const QVector<SketchScrapDebugEntry> empty;
        auto it = m_sketchDebugEntries.constFind(sketch);
        return it != m_sketchDebugEntries.constEnd() ? it.value() : empty;
    }

    const QVector<cwSketchScrapRejectedStroke>&
    sketchRejectedStrokes(cwSketch* sketch) const {
        static const QVector<cwSketchScrapRejectedStroke> empty;
        auto it = m_sketchRejectedStrokes.constFind(sketch);
        return it != m_sketchRejectedStrokes.constEnd() ? it.value() : empty;
    }

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setFutureManagerToken(cwFutureManagerToken token);
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);
    void setSketchManager(cwSketchManager* sketchManager);

    Q_INVOKABLE void setRenderScraps(cwRenderTexturedItems* glScraps);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    void waitForFinish();

    QList<cwScrap*> dirtyScraps() const;

    QList<TriangulatedScrapResult> triangulateScraps(const QList<cwScrap*>& scraps) const;

    cwTriangulateWarping* warpingSettings() const { return m_warpingSettings; }

    bool isTrackingSketch(cwSketch* sketch) const { return m_sketchDerivedScraps.contains(sketch); }
    int  trackedSketchCount() const { return m_sketchDerivedScraps.size(); }

    Q_INVOKABLE int derivedScrapCount(cwSketch* sketch) const { return m_sketchDerivedScraps.value(sketch).size(); }
    Q_INVOKABLE int renderScrapCount() const { return m_scrapToRenderId.size(); }

signals:
    void automaticUpdateChanged();

    // Fires whenever stroke-level state changes for a tracked sketch; downstream
    // consumers (and tests) use this to observe the diff pipeline.
    void sketchDerivedScrapsUpdated(cwSketch* sketch);

    void sketchDiagnosticsChanged(cwSketch* sketch);

public slots:
    void updateAllScraps();

private:
    QPointer<cwRegionTreeModel> RegionModel;
    cwLinePlotManager* LinePlotManager;

    QSet<cwScrap*> DirtyScraps; //These are the scraps that need to be updated
    QSet<cwScrap*> DeletedScraps; //All the deleted scraps
    QHash<cwScrap*, uint32_t> m_scrapToRenderId; //The render id of the scrap
    cwKeywordItemRegistry<cwScrap*> m_keywordRegistry;

    //The task that'll be run
    cwProject* Project;
    AsyncFuture::Restarter<void> TriangulateRestarter;
//    QFuture<void> TriangulateFuture;
    cwFutureManagerToken FutureManagerToken;

    //The render scraps that need updating
    QPointer<cwRenderTexturedItems> m_renderScraps;

    bool AutomaticUpdate; //!< 

    cwTriangulateWarping* m_warpingSettings = nullptr;
    std::unique_ptr<class cwTriangulateWarpingSettings> m_warpingSettingsStore;
    QMetaObject::Connection m_pathReadyConnection;

    // Per-sketch tracking: scraps derived from each sketch's outline
    // strokes, keyed by the outline's sorted member-stroke ids. Keying on
    // the whole set lets a chain survive unrelated stroke edits — only a
    // change to its own membership churns the tracked scrap.
    QHash<cwSketch*, QHash<QVector<QUuid>, cwScrap*>> m_sketchDerivedScraps;

    // Trip-local bounding box per sketch-derived scrap. The scrap stores
    // its outline in normalized [0,1] coords, so the raw world-meter box is
    // kept here — the rasterizer and noteImageResolution both need it at
    // triangulation time.
    QHash<cwScrap*, QRectF> m_sketchScrapBoundingBox;

    QHash<cwSketch*, QVector<SketchScrapDebugEntry>> m_sketchDebugEntries;
    QHash<cwSketch*, QVector<cwSketchScrapRejectedStroke>> m_sketchRejectedStrokes;

    QPointer<cwSketchManager> m_sketchManager;

    // Trip captured at acquireLinePlot() time so release pairs against the
    // same trip pointer even if sketch->parentTrip() later changes or the
    // sketch is reparented before its acquire is balanced.
    QHash<cwSketch*, QPointer<cwTrip>> m_sketchLinePlotTrip;

    void connectNote(cwNote* note);
    void connectScrap(cwScrap* scrap);
    void connectSketch(cwSketch* sketch);

    void disconnectNote(cwNote* note);
    void disconnectScrap(cwScrap* scrap);
    void disconnectSketch(cwSketch* sketch);

    void sketchInsertedHelper(cwSketch* sketch);
    void sketchRemovedHelper(cwSketch* sketch);
    void updateDerivedScrapsForSketch(cwSketch* sketch);
    void releaseSketchLinePlot(cwSketch* sketch);

    // Shared render/keyword hookup + teardown for both note- and sketch-
    // parented scraps. The caller handles connectScrap/disconnectScrap and
    // whatever per-parent ownership semantics apply (note scraps are child
    // QObjects of the note; sketch scraps need an explicit deleteLater).
    void attachScrap(cwScrap* scrap);
    void detachScrap(cwScrap* scrap);

    void updateScrapGeometry(QList<cwScrap *> scraps = QList<cwScrap*>());
    void updateScrapGeometryHelper(QList<cwScrap *> scraps);
    cwTriangulateInData mapScrapToTriangulateInData(cwScrap *scrap) const;
    static QList<cwTriangulateStation> mapNoteStationsToTriangulateStation(QList<cwNoteStation> noteStations,
                                                                           const cwStationPositionLookup& positionLookup);

    void scrapInsertedHelper(cwNote* parentNote, int begin, int end);
    void scrapRemovedHelper(cwNote* parentNote, int begin, int end);
    void addKeywordItemForScrap(cwScrap* scrap);
    void removeKeywordItemForScrap(cwScrap* scrap);

    void updateExistingScrapGeometryHelper(cwScrap* scrap);
    void regenerateScrapGeometryHelper(cwScrap* scrap);

    void addToDeletedScraps(cwScrap* scrap);

    bool scrapImagesOkay(cwScrap* scrap);

    bool isScrapGeometryValid(const cwScrap* scrap) const;

private slots:
    void handleRegionReset();

    void inserted(QModelIndex parent, int begin, int end);
    void removed(QModelIndex parent, int begin, int end);

    void scrapInserted(int begin, int end);
    void scrapRemoved(int begin, int end);

    void regenerateScrapGeometry(cwScrap* scrap);
    void updateScrapPoints(int begin, int end);  //This is called by cwScrap
    void removeScrapPoints(int begin, int end);

    void updateExistingScrapGeometry(); //This is called by cwScrap only
    void updateScrapStations(int begin, int end); //This is called by cwScrap
    void updateScrapStation(int noteStationIndex); //This is called by a cwScrap

    void scrapLeadInserted(int begin, int end);
    void scrapLeadRemoved(int begin, int end);
    void scrapLeadUpdated(int begin, int end, QList<int> roles);

    void updateScrapsWithNewNoteResolution(); //This is called by cwNote
    void updateScrapWithNewNoteTransform(); //This is called by cwNoteTransform

    void updateStationPositionChangedForScraps(QList<cwScrap*> scraps);
    void rerunDirtyScraps();

    void scrapDeleted(QObject* scrap);

    void taskFinished(const QList<cwScrap *> &scrapsToUpdate,
                      const QList<cwTriangulatedData>& scrapDataset);

};


/**
 * @brief qHash
 * @param scrapPointer
 * @return The hash for a weak pointer cwScrap
 */
inline uint32_t qHash(const QWeakPointer<cwScrap> &scrapPointer)
{
    return qHash(scrapPointer.toStrongRef().data());
}

/**
Gets automaticUpdate

 If true the scrap manager automatically update the 3d geometry of the scrap
*/
inline bool cwScrapManager::automaticUpdate() const {
    return AutomaticUpdate;
}




#endif // CWSCRAPMANAGER_H
