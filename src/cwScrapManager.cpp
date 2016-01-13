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
#include "cwTaskManagerModel.h"
#include "cwRegionTreeModel.h"

//Qt includes
#include <QThread>

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    //    Region(nullptr),
    LinePlotManager(nullptr),
    TriangulateThread(new QThread(this)),
    TriangulateTask(new cwTriangulateTask()),
    RemoveImageTask(new cwRemoveImageTask(this)), //Runs in the scrapManager's thread
    Project(nullptr),
    TaskManagerModel(nullptr),
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
  \brief Sets the project for the scrap manager
  */
void cwScrapManager::setProject(cwProject *project) {
    if(project != Project) {
        if(Project != nullptr) {
            disconnect(Project, &cwProject::filenameChanged, this, &cwScrapManager::updateImageProviderPath);
        }

        Project = project;

        if(Project != nullptr) {
            ImageProvider.setProjectPath(Project->filename());

            connect(Project, &cwProject::filenameChanged, this, &cwScrapManager::updateImageProviderPath);
        }
    }
}

/**
 * @brief cwScrapManager::setRegionTreeModel
 * @param regionTreeModel
 *
 * Sets the region tree. This class listens to region tree adds and removes
 */
void cwScrapManager::setRegionTreeModel(cwRegionTreeModel *regionTreeModel)
{
    if(RegionModel != regionTreeModel) {

        if(!RegionModel.isNull()) {
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsInserted,
                       this, &cwScrapManager::inserted);
            disconnect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                       this, &cwScrapManager::removed);
        }

        RegionModel = regionTreeModel;

        if(!RegionModel.isNull()) {
            connect(RegionModel.data(), &cwRegionTreeModel::rowsInserted,
                    this, &cwScrapManager::inserted);
            connect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                    this, &cwScrapManager::removed);
        }

        handleRegionReset();
    }
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
 * @brief cwScrapManager::setTaskManager
 * @param taskManager
 *
 * We use this to expose the task's that are running in this manager to the user. This allows
 * the user to see if the task is running or not running.
 */
void cwScrapManager::setTaskManager(cwTaskManagerModel *taskManager)
{
    if(TaskManagerModel != taskManager) {
        if(TaskManagerModel != nullptr) {
            TaskManagerModel->removeTask(TriangulateTask);
            TaskManagerModel->removeTask(RemoveImageTask);
        }

        TaskManagerModel = taskManager;

        if(TaskManagerModel != nullptr) {
            TaskManagerModel->addTask(TriangulateTask);
            TaskManagerModel->removeTask(RemoveImageTask);
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
    foreach(cwCave* cave, RegionModel->cavingRegion()->caves()) {
        foreach(cwTrip* trip, cave->trips()) {
            foreach(cwNote* note, trip->notes()->notes()) {
                scraps.append(note->scraps());
            }
        }
    }

    updateScrapGeometryHelper(scraps);
}

/**
 * @brief cwScrapManager::updateImageProviderPath
 *
 * This updates the image providers project path
 */
void cwScrapManager::updateImageProviderPath()
{
    ImageProvider.setProjectPath(Project->filename());
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
 * @brief cwScrapManager::scrapImagesOkay
 * @param scrap
 * @return Returns true if the scrap's cropped image is in the database, and false if it's missing
 */
bool cwScrapManager::scrapImagesOkay(cwScrap *scrap)
{
    if(scrap->triangulationData().croppedImage().isValid()) {
        //Should be in the database
        foreach(int mipmap, scrap->triangulationData().croppedImage().mipmaps()) {
            cwImageData imageData = ImageProvider.data(mipmap, true);
            if(!imageData.size().isValid()) {
                return false;
            }
        }
        return true;
    }
    //No cropped image
    return false;
}

/**
 * @brief cwScrapManager::handleRegionReset
 *
 * This should be called if the RegionTreeModel has been reset
 */
void cwScrapManager::handleRegionReset()
{
    if(RegionModel->cavingRegion() != nullptr) {
        foreach(cwCave* cave, RegionModel->cavingRegion()->caves()) {
            foreach(cwTrip* trip, cave->trips()) {
                foreach(cwNote* note, trip->notes()->notes()) {
                    scrapInsertedHelper(note, 0, note->scraps().size() - 1);
                }
            }
        }
    }
}

/**
 * @brief cwScrapManager::inserted
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::inserted(QModelIndex parent, int begin, int end)
{

    if(RegionModel->isTrip(parent) || RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            QModelIndex index = RegionModel->index(i, 0, parent);

            switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
            case cwRegionTreeModel::NoteType: {
                cwNote* note = index.data(cwRegionTreeModel::ObjectRole).value<cwNote*>();
                connectNote(note);
                break;
            }
            }
        }

        if(RegionModel->isNote(parent)) {
            scrapInsertedHelper(RegionModel->note(parent), begin, end);
        }
    }
}

/**
 * @brief cwScrapManager::removed
 * @param parent
 * @param begin
 * @param end
 */
void cwScrapManager::removed(QModelIndex parent, int begin, int end)
{
    if(RegionModel->isTrip(parent) || RegionModel->isNote(parent)) {
        for(int i = begin; i <= end; i++) {
            QModelIndex index = RegionModel->index(i, 0, parent);
            switch(index.data(cwRegionTreeModel::TypeRole).toInt()) {
            case cwRegionTreeModel::NoteType: {
                cwNote* note = index.data(cwRegionTreeModel::ObjectRole).value<cwNote*>();
                disconnectNote(note);
                break;
            }
            }
        }

        if(RegionModel->isNote(parent)) {
            scrapRemovedHelper(RegionModel->note(parent), begin, end);
        }
    }
}

/**
 * @brief cwScrapManager::connectNote
 * @param note
 */
void cwScrapManager::connectNote(cwNote *note) {
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
    connect(scrap, &cwScrap::leadsInserted, this, &cwScrapManager::scrapLeadInserted);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwScrapManager::scrapLeadRemoved);
    connect(scrap, &cwScrap::leadsDataChanged, this, &cwScrapManager::scrapLeadUpdated);
}

/**
 * @brief cwScrapManager::disconnectNote
 * @param note
 */
void cwScrapManager::disconnectNote(cwNote *note)
{
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
    disconnect(scrap, &cwScrap::leadsInserted, this, &cwScrapManager::scrapLeadInserted);
    disconnect(scrap, &cwScrap::leadsRemoved, this, &cwScrapManager::scrapLeadRemoved);
    disconnect(scrap, &cwScrap::leadsDataChanged, this, &cwScrapManager::scrapLeadUpdated);
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

/**
 * @brief cwScrapManager::updateScrapGeometryHelper
 * @param scraps
 */
void cwScrapManager::updateScrapGeometryHelper(QList<cwScrap *> scraps)
{
    if(scraps.isEmpty()) { return; }

    //Union NeedUpdate list with scraps, these are the scraps that need to be updated
    foreach(cwScrap* scrap, scraps) {
        connect(scrap, SIGNAL(destroyed(QObject*)), this, SLOT(scrapDeleted(QObject*)));

        cwTriangulatedData oldData = scrap->triangulationData();
        oldData.setStale(true);
        scrap->setTriangulationData(oldData);

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

    data.setLeads(scrap->leads());

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
//void cwScrapManager::tripsInsertedHelper(cwCave *parentCave, int begin, int end)
//{
//    for(int i = begin; i <= end; i++) {
//        cwTrip* trip = parentCave->trip(i);
//        connectTrip(trip);

//        if(trip->notes()->hasNotes()) {
//            notesInsertedHelper(trip->notes(), QModelIndex(), 0, trip->notes()->rowCount() - 1);
//        }
//    }
//}

/**
 * @brief cwScrapManager::tripsRemovedHelper
 * @param parentCave
 * @param begin
 * @param end
 */
//void cwScrapManager::tripsRemovedHelper(cwCave *parentCave, int begin, int end)
//{
//    for(int i = begin; i <= end; i++) {
//        cwTrip* trip = parentCave->trip(i);
//        disconnectTrip(trip);

//        if(trip->notes()->hasNotes()) {
//            notesRemovedHelper(trip->notes(), QModelIndex(), 0, trip->notes()->rowCount() - 1);
//        }
//    }
//}

/**
 * @brief cwScrapManager::notesInsertedHelper
 * @param noteModel
 * @param parent
 * @param begin
 * @param end
 */
//void cwScrapManager::notesInsertedHelper(cwSurveyNoteModel *noteModel,
//                                         QModelIndex parent,
//                                         int begin,
//                                         int end)
//{
//    Q_UNUSED(parent);

//    for(int i = begin; i <= end; i++) {
//        cwNote* note = noteModel->notes().at(i);
//        connectNote(note);

//        if(note->hasScraps()) {
//            scrapInsertedHelper(note, 0, note->scraps().size() - 1);
//        }
//    }

//}

/**
 * @brief cwScrapManager::notesRemovedHelper
 * @param noteModel
 * @param parent
 * @param begin
 * @param end
 */
//void cwScrapManager::notesRemovedHelper(cwSurveyNoteModel *noteModel,
//                                        QModelIndex parent,
//                                        int begin,
//                                        int end)
//{
//    Q_UNUSED(parent);

//    for(int i = begin; i <= end; i++) {
//        cwNote* note = noteModel->notes().at(i);
//        disconnectNote(note);

//        if(note->hasScraps()) {
//            scrapRemovedHelper(note, 0, note->scraps().size() - 1);
//        }
//    }
//}

/**
 * @brief cwScrapManager::scrapInsertedHelper
 * @param parentNote
 * @param begin
 * @param end
 */
void cwScrapManager::scrapInsertedHelper(cwNote *parentNote, int begin, int end)
{
    QList<cwScrap*> scrapsToUpdate;
    for(int i = begin; i <= end; i++) {
        cwScrap* scrap = parentNote->scrap(i);

        //Connect the scrap
        connectScrap(scrap);

        //Add the scrap data that's already in it
        GLScraps->addScrapToUpdate(scrap);

        //Make sure the scrap's previously calculated data is okay.
        if(scrap->triangulationData().isStale() ||
                scrap->triangulationData().isNull() ||
                !scrapImagesOkay(scrap)
                )
        {
            //Isn't okay, we need to recalculate it
            scrapsToUpdate.append(scrap);
        }
    }

    //Update all the scrap geometry for the scrap
    updateScrapGeometry(scrapsToUpdate);
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
 * @brief cwScrapManager::scrapInserted
 * @param begin
 * @param end
 */
void cwScrapManager::scrapInserted(int begin, int end) {
    Q_ASSERT(qobject_cast<cwNote*>(sender()));
    cwNote* note = static_cast<cwNote*>(sender());
    scrapInsertedHelper(note, begin, end);
}

/**
 * @brief cwScrapManager::scrapRemoved
 * @param begin
 * @param end
 */
void cwScrapManager::scrapRemoved(int begin, int end) {
    Q_ASSERT(qobject_cast<cwNote*>(sender()));
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
 * @brief cwScrapManager::scrapLeadInserted
 * @param begin
 * @param end
 *
 * Called when a lead is added
 */
void cwScrapManager::scrapLeadInserted(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);

    cwScrap* scrap = static_cast<cwScrap*>(sender());
    updateExistingScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::scrapLeadRemoved
 * @param begin
 * @param end
 *
 * Called when a lead is removed
 */
void cwScrapManager::scrapLeadRemoved(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);

    cwScrap* scrap = static_cast<cwScrap*>(sender());
    updateExistingScrapGeometryHelper(scrap);
}

/**
 * @brief cwScrapManager::scrapLeadUpdated
 * @param begin
 * @param end
 * @param roles
 *
 * Called when the lead's data has changed. This only re-runs the scrap if the note position
 * has changed.
 */
void cwScrapManager::scrapLeadUpdated(int begin, int end, QList<int> roles)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);

    cwScrap* scrap = static_cast<cwScrap*>(sender());

    foreach(int role, roles) {
        if(role == cwScrap::LeadPositionOnNote) {
            updateExistingScrapGeometryHelper(scrap);
            break;
        }
    }
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
            Q_ASSERT(!triangleData.isStale());

            scrap->setTriangulationData(triangleData);
            GLScraps->addScrapToUpdate(scrap);
        }
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
