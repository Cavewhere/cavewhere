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
#include <QWeakPointer>
#include <QPointer>
#include <QQmlEngine>
#include <QFuture>

//Our includes
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteModel;
class cwScrap;
class cwNote;
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
class cwKeywordItem;
class cwRenderTexturedItemVisibility;
#include "cwNoteStation.h"
#include "cwTriangulateInData.h"
#include "cwTriangulatedData.h"
#include "cwImageProvider.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"
#include "cwAsyncFuture.h"
#include "cwTriangulateWarping.h"

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

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setFutureManagerToken(cwFutureManagerToken token);
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    Q_INVOKABLE void setRenderScraps(cwRenderTexturedItems* glScraps);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    void waitForFinish();

    QList<cwScrap*> dirtyScraps() const;

    QList<TriangulatedScrapResult> triangulateScraps(const QList<cwScrap*>& scraps) const;

    cwTriangulateWarping* warpingSettings() const { return m_warpingSettings; }

signals:
    void automaticUpdateChanged();

public slots:
    void updateAllScraps();

private:
    QPointer<cwRegionTreeModel> RegionModel;
    cwLinePlotManager* LinePlotManager;

    QSet<cwScrap*> DirtyScraps; //These are the scraps that need to be updated
    QSet<cwScrap*> DeletedScraps; //All the deleted scraps
    QHash<cwScrap*, uint32_t> m_scrapToRenderId; //The render id of the scrap
    struct ScrapKeywordEntry {
        QPointer<cwKeywordItem> item;
        QPointer<cwRenderTexturedItemVisibility> visibility;
    };
    cwKeywordItemModel* m_keywordItemModel = nullptr;
    QHash<cwScrap*, ScrapKeywordEntry> m_scrapKeywordEntries;

    //The task that'll be run
    cwProject* Project;
    cwAsyncFuture::Restarter<void> TriangulateRestarter;
//    QFuture<void> TriangulateFuture;
    cwFutureManagerToken FutureManagerToken;

    //The render scraps that need updating
    QPointer<cwRenderTexturedItems> m_renderScraps;

    bool AutomaticUpdate; //!< 

    cwTriangulateWarping* m_warpingSettings = nullptr;

    void connectNote(cwNote* note);
    void connectScrap(cwScrap* scrap);

    void disconnectNote(cwNote* note);
    void disconnectScrap(cwScrap* scrap);

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
