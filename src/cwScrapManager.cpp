//Our includes
#include "cwScrapManager.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteModel.h"
#include "cwNote.h"
#include "cwScrap.h"
#include "cwTriangulateTask.h"
#include "cwProject.h"
#include "cwTriangulateInData.h"
#include "cwDebug.h"
#include "cwGLScraps.h"
#include "cwRemoveImageTask.h"
#include "cwImageResolution.h"

//Qt includes
#include <QThread>

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    Region(NULL),
    TriangulateThread(new QThread(this)),
    TriangulateTask(new cwTriangulateTask()),
    RemoveImageTask(new cwRemoveImageTask(this)), //Runs in the scrapManager's thread
    Project(NULL),
    GLScraps(NULL)
{
    TriangulateTask->setThread(TriangulateThread);

    connect(TriangulateTask, SIGNAL(finished()), SLOT(taskFinished()));

}

cwScrapManager::~cwScrapManager()
{
    TriangulateThread->quit();
    TriangulateThread->wait();
}

/**
  \brief Set the region for the scrap manager
  */
void cwScrapManager::setRegion(cwCavingRegion* region) {
    if(Region != NULL) {
        disconnect(Region, NULL, this, NULL);
    }

    Region = region;

    if(Region != NULL) {
        //Connect all signal from the region
//        connect(Region, SIGNAL(destroyed(QObject*)), SLOT(regionDestroyed(QObject*)));
//        connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(updateCaveScrapGeometry(int, int)));
//        connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(connectAddedCaves(int,int)));
//        connect(Region, SIGNAL(removedCaves(int,int)), SLOT(updateCaveScrapGeometry(int, int)));

        connectAllCaves();
    }
}

/**
  \brief Sets the project for the scrap manager
  */
void cwScrapManager::setProject(cwProject *project) {
    Project = project;
}

/**
  This function is for testing

  This will gather all the scraps from all the caves, and trips, and notes and regenerate
  all there geometry
  */
void cwScrapManager::updateAllScraps() {

    //Get all the scraps for the whole region
    QList<cwScrap*> scraps;
    foreach(cwCave* cave, Region->caves()) {
        foreach(cwTrip* trip, cave->trips()) {
            foreach(cwNote* note, trip->notes()->notes()) {
                scraps.append(note->scraps());
            }
        }
    }

    updateScrapGeometry(scraps);
}

void cwScrapManager::connectAllCaves() {
    foreach(cwCave* cave, Region->caves()) {
        connectCave(cave);
    }
}

void cwScrapManager::connectCave(cwCave *cave) {
    connect(cave, SIGNAL(insertTrips(int,int)), SLOT(tripsInserted(int,int)));
    connect(cave, SIGNAL(beginRemoveTrips(int,int)), SLOT(tripsRemoved(int,int)));

    foreach(cwTrip* trip, cave->trips()) {
        connectTrip(trip);
    }
}

void cwScrapManager::connectTrip(cwTrip *trip) {
    connectNoteModel(trip->notes());
}

void cwScrapManager::connectNoteModel(cwSurveyNoteModel *noteModel) {
    connect(noteModel, SIGNAL(rowsAboutToBeInserted(QModelIndex, int, int)), SLOT(notesInserted(QModelIndex,int,int)));
    connect(noteModel, SIGNAL(rowsAboutToBeRemoved(QModelIndex, int, int)), SLOT(notesRemoved(QModelIndex,int,int)));

    foreach(cwNote* note, noteModel->notes()) {
        connectNote(note);
    }
}

void cwScrapManager::connectNote(cwNote *note) {
    connect(note, SIGNAL(insertedScraps(int,int)), SLOT(scrapInserted(int,int)));
    connect(note, SIGNAL(beginRemovingScraps(int,int)), SLOT(scrapRemoved(int,int)));

    connectScrapes(note->scraps());
}

void cwScrapManager::connectScrapes(QList<cwScrap*> scraps) {

    //Add all the scraps to the set
    foreach(cwScrap* scrap, scraps) {
        Scraps.insert(scrap);
    }

    //Update all the scrap geometry for the scrap
    updateScrapGeometry(scraps);
}

/**
  This function will run a task that will
  1. Crop out the scrap from the note, with mimap levels
  2. Generate geometry for the scrap
  3. Morph the geometry to the station positions
  */
void cwScrapManager::updateScrapGeometry(QList<cwScrap *> scraps) {
    WaitingForUpdate.clear();
    foreach(cwScrap* scrap, scraps) {
        WaitingForUpdate.append(QWeakPointer<cwScrap>(scrap));
    }

    //Create the scrap data list
    QList<cwTriangulateInData> scrapData;
    foreach(cwScrap* scrap, scraps) {
        qDebug() << "Update scrap geometry for: " << scrap;
        scrapData.append(mapScrapToTriangulateInData(scrap));
    }

    if(TriangulateTask->isReady()) {
        TriangulateTask->setProjectFilename(Project->filename());
        TriangulateTask->setScrapData(scrapData);
        TriangulateTask->start();
    }
}

/**
    Extracts data from the cwScrap and puts it into a cwTriangulateInData
  */
cwTriangulateInData cwScrapManager::mapScrapToTriangulateInData(cwScrap *scrap) const {
    cwTriangulateInData data;
    cwCave* cave = scrap->parentNote()->parentTrip()->parentCave();
    data.setNoteImage(scrap->parentNote()->image());
    data.setOutline(scrap->points());
    data.setStations(mapNoteStationsToTriangulateStation(scrap->stations(), cave->stationPositionLookup()));
    data.setNoteTransform(*(scrap->noteTransformation()));

    double dotsPerMeter = scrap->parentNote()->imageResolution()->convertTo(cwUnits::DotsPerMeter).value();
    data.setNoteImageResolution(dotsPerMeter);

    return data;
}

/**
  Extracts the note station's position and note position
  */
QList<cwTriangulateStation> cwScrapManager::mapNoteStationsToTriangulateStation(QList<cwNoteStation> noteStations,
                                                                                const cwStationPositionLookup& positionLookup) const {
    QList<cwTriangulateStation> stations;
    foreach(cwNoteStation noteStation, noteStations) {
        if(positionLookup.hasPosition(noteStation.name())) {
            cwTriangulateStation station;
            station.setName(noteStation.name());
            station.setNotePosition(noteStation.positionOnNote());
            station.setPosition(positionLookup.position(noteStation.name()));
            stations.append(cwTriangulateStation(station));
        }
    }
    return stations;
}


void cwScrapManager::cavesInserted(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::cavesRemoved(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::tripsInserted(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::tripsRemoved(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::notesInserted(QModelIndex parent, int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
    Q_UNUSED(parent);
}

void cwScrapManager::notesRemoved(QModelIndex parent, int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
    Q_UNUSED(parent);
}

void cwScrapManager::scrapInserted(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::scrapRemoved(int begin, int end) {
    Q_UNUSED(begin);
    Q_UNUSED(end);
}

void cwScrapManager::updateScrapPoints() {

}

/**
  \brief Triangulation task has finished
  */
void cwScrapManager::taskFinished() {
    QList<cwTriangulatedData> scrapDataset = TriangulateTask->triangulatedScrapData();


    //Make sure there's the same amount of data
    if(WaitingForUpdate.size() != scrapDataset.size()) {
        qDebug() << "Scrap size mismatch" << LOCATION;
        return;
    }

    //All the images to remove (replacing the previously calculated or invalid images)
    QList<cwImage> imagesToRemove;

    //Get all the valid scraps
    QList<cwScrap*> validScraps;
    QList<cwTriangulatedData> validScrapTriangleDataset;
    for(int i = 0; i < WaitingForUpdate.size(); i++) {
        QWeakPointer<cwScrap> weakPtrScrap = WaitingForUpdate.at(i);
        cwTriangulatedData triangleData = scrapDataset.at(i);

        //Make sure the scrap still exists
        //FIXME: This will not work with UNDO/REDO because the pointer will still be valid,
        // even through it's remove in the undo stack
        if(!weakPtrScrap.isNull()) {
            validScraps.append(weakPtrScrap.data());
            validScrapTriangleDataset.append(triangleData);
        } else {
            //Remove the cropped image that's not needed
            imagesToRemove.append(triangleData.croppedImage());
        }
    }

    foreach(cwScrap* scrap, validScraps) {
        cwImage image = scrap->triangulationData().croppedImage();
        if(image.isValid()) {
            imagesToRemove.append(image);
        }
    }

    RemoveImageTask->setImagesToRemove(imagesToRemove);
    RemoveImageTask->setDatabaseFilename(Project->filename());
    RemoveImageTask->start(); //This runs in this thread, should be very quick

    for(int i = 0; i < validScraps.size(); i++) {
        cwScrap* scrap = validScraps.at(i);
        cwTriangulatedData triangleData = validScrapTriangleDataset.at(i);
        scrap->setTriangulationData(triangleData);

        //TODO: For debugging should be removed
        QList<cwImage> images;
        images.append(scrapDataset[i].croppedImage());

        scrap->parentNote()->parentTrip()->notes()->addNotesWithNewImages(images);
    }

    GLScraps->setCavingRegion(Region);
    GLScraps->updateGeometry();

    qDebug() << "Task finished!";
}

/**
  \brief Sets the gl scraps for the manager
  */
void cwScrapManager::setGLScraps(cwGLScraps *glScraps)
{
    GLScraps = glScraps;
    GLScraps->setProject(Project);
}
