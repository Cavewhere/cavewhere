#ifndef CWSCRAPMANAGER_H
#define CWSCRAPMANAGER_H

//Qt includes
#include <QObject>
#include <QModelIndex>
#include <QSet>
#include <QWeakPointer>

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
#include "cwNoteStation.h"
#include "cwTriangulateInData.h"

/**
    The scrap manager listens to changes in the notes and creates all
    the geometry need to show a scrap in 3d
  */
class cwScrapManager : public QObject
{
    Q_OBJECT
public:
    explicit cwScrapManager(QObject *parent = 0);
    ~cwScrapManager();

    void setRegion(cwCavingRegion* region);
    void setProject(cwProject* project);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);

    Q_INVOKABLE void setGLScraps(cwGLScraps* glScraps);

signals:
    
public slots:
    void updateAllScraps();

private:
    cwCavingRegion* Region;
    cwLinePlotManager* LinePlotManager;

    QList<QWeakPointer<cwScrap> > WaitingForUpdate; //These are the scraps that are running through task
    QSet<QWeakPointer<cwScrap> > DirtyScraps; //These are the scraps that need to be updated

    //The task that'll be run
    QThread* TriangulateThread;
    cwTriangulateTask* TriangulateTask;
    cwRemoveImageTask* RemoveImageTask;
    cwProject* Project;

    //For testing only
    cwGLScraps* GLScraps;

    void connectCave(cwCave* cave);
    void connectTrip(cwTrip* trip);
    void connectNoteModel(cwSurveyNoteModel* noteModel);
    void connectNote(cwNote* note);
    void connectScrap(cwScrap* scrap);

    void disconnectCave(cwCave* cave);
    void disconnectTrip(cwTrip* trip);
    void disconnectNoteModel(cwSurveyNoteModel* noteModel);
    void disconnectNote(cwNote* note);
    void disconnectScrap(cwScrap* scrap);

    void updateScrapGeometry(QList<cwScrap *> scraps = QList<cwScrap*>());
    cwTriangulateInData mapScrapToTriangulateInData(cwScrap *scrap) const;
    QList<cwTriangulateStation> mapNoteStationsToTriangulateStation(QList<cwNoteStation> noteStations, const cwStationPositionLookup& positionLookup) const;

    void tripsInsertedHelper(cwCave* parentCave, int begin, int end);
    void tripsRemovedHelper(cwCave* parentCave, int begin, int end);
    void notesInsertedHelper(cwSurveyNoteModel* noteModel, QModelIndex parent, int begin, int end);
    void notesRemovedHelper(cwSurveyNoteModel* noteModel, QModelIndex parent, int begin, int end);
    void scrapInsertedHelper(cwNote* parentNote, int begin, int end);
    void scrapRemovedHelper(cwNote* parentNote, int begin, int end);

    void updateExistingScrapGeometryHelper(cwScrap* scrap);
    void regenerateScrapGeometryHelper(cwScrap* scrap);

private slots:
    void cavesInserted(int begin, int end);
    void cavesRemoved(int begin, int end);
    void tripsInserted(int begin, int end);
    void tripsRemoved(int begin, int end);
    void notesInserted(QModelIndex parent, int begin, int end);
    void notesRemoved(QModelIndex parent, int begin, int end);
    void scrapInserted(int begin, int end);
    void scrapRemoved(int begin, int end);

    void regenerateScrapGeometry(); //This is called by cwScrap
    void updateScrapPoints(int begin, int end);  //This is called by cwScrap

    void updateExistingScrapGeometry(); //This is called by cwScrap only
    void updateScrapStations(int begin, int end); //This is called by cwScrap
    void updateScrapStation(int noteStationIndex); //This is called by a cwScrap

    void updateScrapsWithNewNoteResolution(); //This is called by cwNote
    void updateScrapWithNewNoteTransform(); //This is called by cwNoteTransform

    void updateStationPositionChangedForScraps(QList<cwScrap*> scraps);
    void rerunDirtyScraps();

    void taskFinished();

};

/**
 * @brief qHash
 * @param scrapPointer
 * @return The hash for a weak pointer cwScrap
 */
inline uint qHash(const QWeakPointer<cwScrap> &scrapPointer)
{
    return qHash(scrapPointer.data());
}




#endif // CWSCRAPMANAGER_H
