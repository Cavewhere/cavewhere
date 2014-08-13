/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

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
#include "cwLinePlotManager.h"

//Qt includes
#include <QThread>

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    Region(nullptr),
    LinePlotManager(nullptr),
    TriangulateThread(new QThread(this)),
    TriangulateTask(new cwTriangulateTask()),
    RemoveImageTask(new cwRemoveImageTask(this)), //Runs in the scrapManager's thread
    Project(nullptr),
    GLScraps(nullptr),
    AutomaticUpdate(true)
{
    TriangulateTask->setThread(TriangulateThread);

    connect(TriangulateTask, SIGNAL(finished()), SLOT(taskFinished()));
    connect(TriangulateTask, &cwTriangulateTask::shouldRerun, this, &cwScrapManager::rerunDirtyScraps);

}

cwScrapManager::~cwScrapManager()
{
    TriangulateTask->stop();
    TriangulateThread->quit();
    TriangulateThread->wait();
}

/**
  \brief Set the region for the scrap manager
  */
void cwScrapManager::setRegion(cwCavingRegion* region) {
    if(Region != nullptr) {
        disconnect(Region, nullptr, this, nullptr);
    }

    Region = region;

    if(Region != nullptr) {
//        connect(Region, SIGNAL(destroyed(QObject*)), SLOT(regionDestroyed(QObject*)));
//        connect(Region, SIGNAL(insertedCaves(int,int)), SLOT(connectAddedCaves(int,int)));
        connect(Region, &cwCavingRegion::insertedCaves, this, &cwScrapManager::cavesInserted);
        connect(Region, &cwCavingRegion::beginRemoveCaves, this, &cwScrapManager::cavesRemoved);
//        connect(Region, SIGNAL(removedCaves(int,int)), SLOT(updateCaveScrapGeometry(int, int)));

        if(Region->hasCaves()) {
            cavesInserted(0, Region->caveCount() - 1);
        }
    }
}

/**
  \brief Sets the project for the scrap manager
  */
void cwScrapManager::setProject(cwProject *project) {
    Project = project;
}

/**
 * @brief cwScrapManager::setLinePlotManager
 * @param linePlotManager
 *
 * Sets the line plot manager for the scrap
 */
void cwScrapManager::setLinePlotManager(cwLinePlotManager *linePlotManager)
{
    if(LinePlotManager != linePlotManager) {
        if(LinePlotManager != nullptr) {
            disconnect(LinePlotManager, &cwLinePlotManager::stationPositionInScrapsChanged,
                    this, &cwScrapManager::updateStationPositionChangedForScraps);
        }

        LinePlotManager = linePlotManager;

        if(LinePlotManager != nullptr) {
            connect(LinePlotManager, &cwLinePlotManager::stationPositionInScrapsChanged,
                    this, &cwScrapManager::updateStationPositionChangedForScraps);
        }
    }
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

    updateScrapGeometryHelper(scraps);
}

/**
 * @brief cwScrapManager::updateScrapGeometryWithStationPositions
 * @param scraps
 *
 * This only remorphs the scraps geometry.  This doesn't cut out the scrap.
 */
void cwScrapManager::updateStationPositionChangedForScraps(QList<cwScrap *> scraps)
{
    //Update all the note transformation, because the station positions have changed
    foreach(cwScrap* scrap, scraps) {
        disconnectScrap(scrap);

        scrap->updateNoteTransformation();

        connectScrap(scrap);
    }

    //Update the scrap geometry
    //FIXME: We should only morph the existing geometry
    updateScrapGeometry(scraps);  //Explict call, don't check automatic update
}

void cwScrapManager::rerunDirtyScraps()
{
    updateScrapGeometry(DirtyScraps.toList());
}

/**
 * @brief cwScrapManager::scrapDeleted
 * @param scrap, called when the scrap was delete
 */
void cwScrapManager::scrapDeleted(QObject *scrapObj)
{
    cwScrap* scrap = static_cast<cwScrap*>(scrapObj);
    addToDeletedScraps(scrap);
    DirtyScraps.remove(scrap); //scrapObj);
}

/**
 * @brief cwScrapManager::updateExistingScrapGeometryHelper
 * @param scrap
 *
 * Updates the scrap with the existing geometry.  This just remorphs the scrap
 */
void cwScrapManager::updateExistingScrapGeometryHelper(cwScrap *scrap)
{
    QList<cwScrap*> scraps;
    scraps.append(scrap);
    updateScrapGeometry(scraps);
}

/**
 * @brief cwScrapManager::updateScrapGeometryHelper
 * @param scrap
 *
 * Updates the whole scrap, cuts out the texture again, and remorphs the scrap
 */
void cwScrapManager::regenerateScrapGeometryHelper(cwScrap *scrap)
{
    QList<cwScrap*> scraps;
    scraps.append(scrap);
    updateScrapGeometry(scraps);
}

void cwScrapManager::addToDeletedScraps(cwScrap *scrap)
{
    if(TriangulateTask->isRunning()) {
        DeletedScraps.insert(scrap);
    }
}

/**
 * @brief cwScrapManager::connectCave
 * @param cave
 */
void cwScrapManager::connectCave(cwCave *cave) {
    connect(cave, &cwCave::insertedTrips, this, &cwScrapManager::tripsInserted);
    connect(cave, &cwCave::beginRemoveTrips, this, &cwScrapManager::tripsRemoved);
}

/**
 * @brief cwScrapManager::connectTrip
 * @param trip
 */
void cwScrapManager::connectTrip(cwTrip *trip) {
    connectNoteModel(trip->notes());
}

/**
 * @brief cwScrapManager::connectNoteModel
 * @param noteModel
 */
void cwScrapManager::connectNoteModel(cwSurveyNoteModel *noteModel) {
    connect(noteModel, &cwSurveyNoteModel::rowsInserted, this, &cwScrapManager::notesInserted);
    connect(noteModel, &cwSurveyNoteModel::rowsAboutToBeRemoved, this, &cwScrapManager::notesRemoved);
}

/**
 * @brief cwScrapManager::connectNote
 * @param note
 */
void cwScrapManager::connectNote(cwNote *note) {
    connect(note, &cwNote::insertedScraps, this, &cwScrapManager::scrapInserted);
    connect(note, &cwNote::beginRemovingScraps, this, &cwScrapManager::scrapRemoved);

    connect(note->imageResolution(), &cwImageResolution::valueChanged, this, &cwScrapManager::updateScrapsWithNewNoteResolution);
    connect(note->imageResolution(), &cwImageResolution::unitChanged, this, &cwScrapManager::updateScrapsWithNewNoteResolution);
}

/**
 * @brief cwScrapManager::connectScrapes
 * @param scraps
 */
void cwScrapManager::connectScrap(cwScrap* scrap) {
    connect(scrap->noteTransformation(), &cwNoteTranformation::scaleChanged, this, &cwScrapManager::updateScrapWithNewNoteTransform); //Morph only
    connect(scrap->noteTransformation(), &cwNoteTranformation::northUpChanged, this, &cwScrapManager::updateScrapWithNewNoteTransform);
    connect(scrap, &cwScrap::insertedPoints, this, &cwScrapManager::updateScrapPoints);
    connect(scrap, &cwScrap::removedPoints, this, &cwScrapManager::updateScrapPoints);
    connect(scrap, &cwScrap::pointChanged, this, &cwScrapManager::updateScrapPoints);
    connect(scrap, &cwScrap::pointsReset, this, &cwScrapManager::regenerateScrapGeometry);
    connect(scrap, &cwScrap::stationAdded, this, &cwScrapManager::updateExistingScrapGeometry);
    connect(scrap, &cwScrap::stationPositionChanged, this, &cwScrapManager::updateScrapStations);
    connect(scrap, &cwScrap::stationRemoved, this, &cwScrapManager::updateScrapStation);
    connect(scrap, &cwScrap::stationNameChanged, this, &cwScrapManager::updateScrapStation);
}

/**
 * @brief cwScrapManager::disconnectCave
 * @param cave
 */
void cwScrapManager::disconnectCave(cwCave *cave)
{
    disconnect(cave, &cwCave::insertedTrips, this, &cwScrapManager::tripsInserted);
    disconnect(cave, &cwCave::beginRemoveTrips, this, &cwScrapManager::tripsRemoved);
}

/**
 * @brief cwScrapManager::disconnectTrip
 * @param trip
 */
void cwScrapManager::disconnectTrip(cwTrip *trip)
{
    Q_UNUSED(trip);
}

/**
 * @brief cwScrapManager::disconnectNoteModel
 * @param noteModel
 */
void cwScrapManager::disconnectNoteModel(cwSurveyNoteModel *noteModel)
{
    disconnect(noteModel, &cwSurveyNoteModel::rowsInserted, this, &cwScrapManager::notesInserted);
    disconnect(noteModel, &cwSurveyNoteModel::rowsAboutToBeRemoved, this, &cwScrapManager::notesRemoved);
}

/**
 * @brief cwScrapManager::disconnectNote
 * @param note
 */
void cwScrapManager::disconnectNote(cwNote *note)
{
    disconnect(note, &cwNote::insertedScraps, this, &cwScrapManager::scrapInserted);
    disconnect(note, &cwNote::beginRemovingScraps, this, &cwScrapManager::scrapRemoved);
    disconnect(note, &cwNote::imageResolutionChanged, this, &cwScrapManager::updateScrapsWithNewNoteResolution);
}

/**
 * @brief cwScrapManager::disconnectScrapes
 * @param scraps
 */
void cwScrapManager::disconnectScrap(cwScrap* scrap)
{
    disconnect(scrap->noteTransformation(), &cwNoteTranformation::scaleChanged, this, &cwScrapManager::updateExistingScrapGeometry); //Morph only
    disconnect(scrap->noteTransformation(), &cwNoteTranformation::northUpChanged, this, &cwScrapManager::updateExistingScrapGeometry);
    disconnect(scrap, &cwScrap::insertedPoints, this, &cwScrapManager::updateScrapPoints);
    disconnect(scrap, &cwScrap::removedPoints, this, &cwScrapManager::updateScrapPoints);
    disconnect(scrap, &cwScrap::pointChanged, this, &cwScrapManager::updateScrapPoints);
    disconnect(scrap, &cwScrap::pointsReset, this, &cwScrapManager::regenerateScrapGeometry);
    disconnect(scrap, &cwScrap::stationAdded, this, &cwScrapManager::updateExistingScrapGeometry);
    disconnect(scrap, &cwScrap::stationPositionChanged, this, &cwScrapManager::updateScrapStations);
    disconnect(scrap, &cwScrap::stationRemoved, this, &cwScrapManager::updateScrapStation);
    disconnect(scrap, &cwScrap::stationNameChanged, this, &cwScrapManager::updateScrapStation);
}

/**
  This function will run a task that will
  1. Crop out the scrap from the note, with mimap levels
  2. Generate geometry for the scrap
  3. Morph the geometry to the station positions
  */
void cwScrapManager::updateScrapGeometry(QList<cwScrap *> scraps) {
    if(AutomaticUpdate) {
        updateScrapGeometryHelper(scraps);
    }
}

void cwScrapManager::updateScrapGeometryHelper(QList<cwScrap *> scraps)
{
    if(scraps.isEmpty()) { return; }

    //Union NeedUpdate list with scraps, these are the scraps that need to be updated
    foreach(cwScrap* scrap, scraps) {
        connect(scrap, SIGNAL(destroyed(QObject*)), this, SLOT(scrapDeleted(QObject*)));
        DirtyScraps.insert(scrap);
    }

    if(TriangulateTask->isReady()) {
        //Running
        QList<cwTriangulateInData> scrapData;
        WaitingForUpdate.clear();

        foreach(cwScrap* scrap, scraps) {
            WaitingForUpdate.append(scrap);
            scrapData.append(mapScrapToTriangulateInData(scrap));
        }

        TriangulateTask->setProjectFilename(Project->filename());
        TriangulateTask->setScrapData(scrapData);
        TriangulateTask->start();
    } else {
        //Isn't ready!, restart the task
        TriangulateTask->restart();
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

/**
 * @brief cwScrapManager::tripsInsertedHelper
 * @param parentCave
 * @param begin
 * @param end
 */
void cwScrapManager::tripsInsertedHelper(cwCave *parentCave, int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentCave->trip(i);
        connectTrip(trip);

        if(trip->notes()->hasNotes()) {
            notesInsertedHelper(trip->notes(), QModelIndex(), 0, trip->notes()->rowCount() - 1);
        }
    }
}

/**
 * @brief cwScrapManager::tripsRemovedHelper
 * @param parentCave
 * @param begin
 * @param end
 */
void cwScrapManager::tripsRemovedHelper(cwCave *parentCave, int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwTrip* trip = parentCave->trip(i);
        disconnectTrip(trip);

        if(trip->notes()->hasNotes()) {
            notesRemovedHelper(trip->notes(), QModelIndex(), 0, trip->notes()->rowCount() - 1);
        }
    }
}

/**
 * @brief cwScrapManager::notesInsertedHelper
 * @param noteModel
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::notesInsertedHelper(cwSurveyNoteModel *noteModel,
                                         QModelIndex parent,
                                         int begin,
                                         int end)
{
    Q_UNUSED(parent);

    for(int i = begin; i <= end; i++) {
        cwNote* note = noteModel->notes().at(i);
        connectNote(note);

        if(note->hasScraps()) {
            scrapInsertedHelper(note, 0, note->scraps().size() - 1);
        }
    }

}

/**
 * @brief cwScrapManager::notesRemovedHelper
 * @param noteModel
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::notesRemovedHelper(cwSurveyNoteModel *noteModel,
                                        QModelIndex parent,
                                        int begin,
                                        int end)
{
    Q_UNUSED(parent);

    for(int i = begin; i <= end; i++) {
        cwNote* note = noteModel->notes().at(i);
        disconnectNote(note);

        if(note->hasScraps()) {
            scrapRemovedHelper(note, 0, note->scraps().size() - 1);
        }
    }
}

/**
 * @brief cwScrapManager::scrapInsertedHelper
 * @param parentNote
 * @param begin
 * @param end
 */
void cwScrapManager::scrapInsertedHelper(cwNote *parentNote, int begin, int end)
{
    QList<cwScrap*> newScraps;
    for(int i = begin; i <= end; i++) {
        cwScrap* scrap = parentNote->scrap(i);

        //Connect the scrap
        connectScrap(scrap);

        //Add the scrap data that's already in it
        GLScraps->addScrapToUpdate(scrap);
    }

    //Update all the scrap geometry for the scrap
    updateScrapGeometry(newScraps);
}

/**
 * @brief cwScrapManager::scrapRemovedHelper
 * @param parentNote
 * @param begin
 * @param end
 */
void cwScrapManager::scrapRemovedHelper(cwNote *parentNote, int begin, int end)
{
    for(int i = begin; i <= end; i++) {
        cwScrap* scrap = parentNote->scrap(i);
        addToDeletedScraps(scrap);

        //Connect the scrap
        disconnectScrap(scrap);

        GLScraps->removeScrap(scrap);
    }
}


/**
 * @brief cwScrapManager::cavesInserted
 * @param begin
 * @param end
 *
 * This is called when the region has new caves that have been inserted
 */
void cwScrapManager::cavesInserted(int begin, int end) {
    //Go through all the caves
    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);
        connectCave(cave);

        if(cave->hasTrips()) {
            int lastTripIndex = cave->tripCount() - 1;
            tripsInsertedHelper(cave, 0, lastTripIndex);
        }
    }
}

/**
 * @brief cwScrapManager::cavesRemoved
 * @param begin
 * @param end
 *
 * Removes caves from the scrap manager, this should be called before.
 *
 * This needs to be called before cave is really remove.  It still needs to be valid
 */
void cwScrapManager::cavesRemoved(int begin, int end) {
    //Go remove all scrap froms the removed caves
    for(int i = begin; i <= end; i++) {
        cwCave* cave = Region->cave(i);

        disconnectCave(cave);

        if(cave->hasTrips()) {
            int lastTripIndex = cave->tripCount() - 1;
            tripsRemovedHelper(cave, 0, lastTripIndex);
        }
    }
}

/**
 * @brief cwScrapManager::tripsInserted
 * @param begin
 * @param end
 *
 * Insert all the trips
 */
void cwScrapManager::tripsInserted(int begin, int end) {
    cwCave* cave = static_cast<cwCave*>(sender());
    tripsInsertedHelper(cave, begin, end);
}

/**
 * @brief cwScrapManager::tripsRemoved
 * @param begin
 * @param end
 */
void cwScrapManager::tripsRemoved(int begin, int end) {
    cwCave* cave = static_cast<cwCave*>(sender());
    tripsRemovedHelper(cave, begin, end);
}

/**
 * @brief cwScrapManager::notesInserted
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::notesInserted(QModelIndex parent, int begin, int end) {
    cwSurveyNoteModel* noteModel = static_cast<cwSurveyNoteModel*>(sender());
    notesInsertedHelper(noteModel, parent, begin, end);
}

/**
 * @brief cwScrapManager::notesRemoved
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::notesRemoved(QModelIndex parent, int begin, int end) {
    cwSurveyNoteModel* noteModel = static_cast<cwSurveyNoteModel*>(sender());
    notesRemovedHelper(noteModel, parent, begin, end);
}

/**
 * @brief cwScrapManager::scrapInserted
 * @param begin
 * @param end
 */
void cwScrapManager::scrapInserted(int begin, int end) {
    cwNote* note = static_cast<cwNote*>(sender());
    scrapInsertedHelper(note, begin, end);
}

/**
 * @brief cwScrapManager::scrapRemoved
 * @param begin
 * @param end
 */
void cwScrapManager::scrapRemoved(int begin, int end) {
    cwNote* note = static_cast<cwNote*>(sender());
    scrapRemovedHelper(note, begin, end);
}

/**
 * @brief cwScrapManager::regenerateScrapGeometry
 *
 * This function should only be called from a cwScrap connect statement
 *
 * This will cause the sender (a cwScrap*) to regenerate the texture and geometry for the
 * scrap.
 */
void cwScrapManager::regenerateScrapGeometry()
{
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    regenerateScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::updateScrapPoints
 * @param begin
 * @param end
 */
void cwScrapManager::updateScrapPoints(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    regenerateScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::updateScrapStations
 * @param begin
 * @param end
 */
void cwScrapManager::updateScrapStations(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    updateExistingScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::updateScrapStation
 * @param noteStationIndex
 */
void cwScrapManager::updateScrapStation(int noteStationIndex)
{
    Q_UNUSED(noteStationIndex);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    updateExistingScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::updateExistingScrapGeometry
 */
void cwScrapManager::updateExistingScrapGeometry()
{
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    updateExistingScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::updateScrapsWithNewNoteResolution
 */
void cwScrapManager::updateScrapsWithNewNoteResolution()
{
    cwImageResolution* resolution = static_cast<cwImageResolution*>(sender());
    cwNote* note = dynamic_cast<cwNote*>(resolution->parent());

    if(note == nullptr) {
        qDebug() << "Resolution doesn't have a note parent" << LOCATION;
        return;
    }

    foreach(cwScrap* scrap, note->scraps()) {
        updateExistingScrapGeometryHelper(scrap);
    }
}

void cwScrapManager::updateScrapWithNewNoteTransform()
{
    cwNoteTranformation* noteTransform = static_cast<cwNoteTranformation*>(sender());
    cwScrap* parentScrap = dynamic_cast<cwScrap*>(noteTransform->parent());
    if(parentScrap == nullptr) {
        qDebug() << "Notetransform's parent isn't a cwScrap" << LOCATION;
        return;
    }

    updateExistingScrapGeometryHelper(parentScrap);
}

/**
  \brief Triangulation task has finished
  */
void cwScrapManager::taskFinished() {
    if(TriangulateTask->isReady()) {

        QList<cwTriangulatedData> scrapDataset = TriangulateTask->triangulatedScrapData();

        if(scrapDataset.isEmpty()) {
            //No scrap data udpated...
            return;
        }

        //Clear all the scraps that need to be update, because we are updating now
        foreach(cwScrap* scrap, DirtyScraps) {
            disconnect(scrap, SIGNAL(destroyed(QObject*)), this, SLOT(scrapDeleted(QObject*)));
        }
        DirtyScraps.clear();

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
            cwScrap* scrap = WaitingForUpdate.at(i);
            cwTriangulatedData triangleData = scrapDataset.at(i);
            if(!DeletedScraps.contains(scrap)) {
                validScraps.append(scrap);
                validScrapTriangleDataset.append(triangleData);
            } else {
                //Scrap has been delete
                imagesToRemove.append(triangleData.croppedImage());
                continue;
            }
        }

        DeletedScraps.clear();

        //Removed all cropped image data
        foreach(cwScrap* scrap, validScraps) {
            cwImage image = scrap->triangulationData().croppedImage();
            qDebug() << "Image is valid:" << image.isValid();
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
            GLScraps->addScrapToUpdate(scrap);
        }

        qDebug() << "Task finished!";
    }
}

/**
  \brief Sets the gl scraps for the manager
  */
void cwScrapManager::setGLScraps(cwGLScraps *glScraps)
{
    GLScraps = glScraps;
    GLScraps->setProject(Project);
}

/**
    Sets automaticUpdate

    If true (default) this class will update the 3d geometry of the scrap when data has
    change.  Otherwise, scraps must be updated manaually, using updateAllScraps().
*/
void cwScrapManager::setAutomaticUpdate(bool automaticUpdate) {
    if(AutomaticUpdate != automaticUpdate) {
        AutomaticUpdate = automaticUpdate;
        emit automaticUpdateChanged();
    }
}
