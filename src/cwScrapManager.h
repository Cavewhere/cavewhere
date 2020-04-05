/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWSCRAPMANAGER_H
#define CWSCRAPMANAGER_H

//Qt includes
#include <QObject>
#include <QModelIndex>
#include <QSet>
#include <QWeakPointer>
#include <QPointer>

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
#include "cwNoteStation.h"
#include "cwTriangulateInData.h"
#include "cwImageProvider.h"
#include "cwFutureManagerToken.h"
#include "cwGlobals.h"

/**
    The scrap manager listens to changes in the notes and creates all
    the geometry need to show a scrap in 3d
  */
class CAVEWHERE_LIB_EXPORT cwScrapManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)

public:
    explicit cwScrapManager(QObject *parent = 0);
    ~cwScrapManager();

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setFutureManagerToken(cwFutureManagerToken token);

    Q_INVOKABLE void setGLScraps(cwGLScraps* glScraps);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    void waitForFinish();

signals:
    void automaticUpdateChanged();

public slots:
    void updateAllScraps();

private:
    QPointer<cwRegionTreeModel> RegionModel;
    cwLinePlotManager* LinePlotManager;

    QSet<cwScrap*> DirtyScraps; //These are the scraps that need to be updated
    QSet<cwScrap*> DeletedScraps; //All the deleted scraps

    //The task that'll be run
    cwProject* Project;
    QFuture<void> TriangulateFuture;
    cwFutureManagerToken FutureManagerToken;

    //The gl scraps that need updating
    cwGLScraps* GLScraps;

    bool AutomaticUpdate; //!<

    void connectNote(cwNote* note);
    void connectScrap(cwScrap* scrap);

    void disconnectNote(cwNote* note);
    void disconnectScrap(cwScrap* scrap);

    void updateScrapGeometry(QList<cwScrap *> scraps = QList<cwScrap*>());
    void updateScrapGeometryHelper(QList<cwScrap *> scraps);
    static cwTriangulateInData mapScrapToTriangulateInData(cwScrap *scrap);
    static QList<cwTriangulateStation> mapNoteStationsToTriangulateStation(QList<cwNoteStation> noteStations,
                                                                           const cwStationPositionLookup& positionLookup);

    void scrapInsertedHelper(cwNote* parentNote, int begin, int end);
    void scrapRemovedHelper(cwNote* parentNote, int begin, int end);

    void updateExistingScrapGeometryHelper(cwScrap* scrap);
    void regenerateScrapGeometryHelper(cwScrap* scrap);

    void addToDeletedScraps(cwScrap* scrap);

    bool scrapImagesOkay(cwScrap* scrap);

private slots:
    void handleRegionReset();

    void inserted(QModelIndex parent, int begin, int end);
    void removed(QModelIndex parent, int begin, int end);

    void scrapInserted(int begin, int end);
    void scrapRemoved(int begin, int end);

    void regenerateScrapGeometry(); //This is called by cwScrap
    void updateScrapPoints(int begin, int end);  //This is called by cwScrap

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
inline uint qHash(const QWeakPointer<cwScrap> &scrapPointer)
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
