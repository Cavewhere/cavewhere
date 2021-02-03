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
#include "cwImageResolution.h"
#include "cwLinePlotManager.h"
#include "cwTaskManagerModel.h"
#include "cwRegionTreeModel.h"
#include "cwScrapsEntity.h"
#include "cwOpenGLSettings.h"
#include "cwImageDatabase.h"
#include "cwAsyncFuture.h"
#include "cwAbstractScrapViewMatrix.h"

//Async future
#include "asyncfuture.h"

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    LinePlotManager(nullptr),
    Project(nullptr),
    ScrapsEntity(new cwScrapsEntity()),
    GLScraps(nullptr),
    AutomaticUpdate(true)
{
}

cwScrapManager::~cwScrapManager()
{
    if(!ScrapsEntity.isNull() && !ScrapsEntity->parentEntity()) {
        delete ScrapsEntity;
    }

    TriangulateFuture.cancel();
    waitForFinish();
}

/**
  \brief Sets the project for the scrap manager
  */
void cwScrapManager::setProject(cwProject *project) {
    if(Project != project) {
        if(Project) {
           disconnect(Project, nullptr, this, nullptr);
        }

        Project = project;

        if(Project) {
            auto updateProfileFilename = [this]() {
                ScrapsEntity->setProject(Project->filename());
            };

            connect(Project, &cwProject::filenameChanged,
                    this, updateProfileFilename);

            updateProfileFilename();
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

cwScrapsEntity* cwScrapManager::scrapsEntity() const {
    return ScrapsEntity;
}


void cwScrapManager::setFutureManagerToken(cwFutureManagerToken token)
{
    FutureManagerToken = token;
    if(GLScraps) {
        GLScraps->setFutureManagerToken(token);
    }
}

void cwScrapManager::setKeywordItemModel(cwKeywordItemModel *keywordItemModel)
{
    ScrapsEntity->setKeywordItemModel(keywordItemModel);
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
                for(cwScrap* scrap : note->scraps()) {
                    DirtyScraps.insert(scrap);
                }
            }
        }
    }

    updateScrapGeometryHelper(cw::toList(DirtyScraps));
    ScrapsEntity->setProject(Project->filename());
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
    updateScrapGeometry({DirtyScraps.begin(), DirtyScraps.end()});
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
    DeletedScraps.insert(scrap);
}

/**
 * @brief cwScrapManager::scrapImagesOkay
 * @param scrap
 * @return Returns true if the scrap's cropped image is in the database, and false if it's missing
 */
bool cwScrapManager::scrapImagesOkay(cwScrap *scrap)
{
    auto image = scrap->triangulationData().croppedImage();
    return cwImageDatabase(Project->filename()).mipmapsValid(image,
                                                             cwOpenGLSettings::instance()->useDXT1Compression());
}

bool cwScrapManager::isScrapGeometryValid(const cwScrap *scrap) const
{
    if(scrap) {
        return scrap->numberOfStations() > 0 && scrap->numberOfPoints() > 0;
    }
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
    connect(scrap, &cwScrap::removedPoints, this, &cwScrapManager::removeScrapPoints);
    connect(scrap, &cwScrap::pointChanged, this, &cwScrapManager::updateScrapPoints);
    connect(scrap, &cwScrap::pointsReset, this, [this, scrap](){ cwScrapManager::regenerateScrapGeometry(scrap); });
    connect(scrap, &cwScrap::stationAdded, this, &cwScrapManager::updateExistingScrapGeometry);
    connect(scrap, &cwScrap::stationPositionChanged, this, &cwScrapManager::updateScrapStations);
    connect(scrap, &cwScrap::stationRemoved, this, &cwScrapManager::updateScrapStation);
    connect(scrap, &cwScrap::stationNameChanged, this, &cwScrapManager::updateScrapStation);
    connect(scrap, &cwScrap::leadsInserted, this, &cwScrapManager::scrapLeadInserted);
    connect(scrap, &cwScrap::leadsRemoved, this, &cwScrapManager::scrapLeadRemoved);
    connect(scrap, &cwScrap::leadsDataChanged, this, &cwScrapManager::scrapLeadUpdated);

    auto connectViewMatrix = [scrap, this]() {
        connect(scrap->viewMatrix(), &cwAbstractScrapViewMatrix::matrixChanged,
                this, [this, scrap]()
        {
            regenerateScrapGeometry(scrap);
        });
    };

    connect(scrap, &cwScrap::viewMatrixChanged,
            this, [this, scrap, connectViewMatrix]()
    {
        connectViewMatrix();
        regenerateScrapGeometry(scrap);
    });

    connectViewMatrix();
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
    disconnect(scrap, &cwScrap::removedPoints, this, &cwScrapManager::removeScrapPoints);
    disconnect(scrap, &cwScrap::pointChanged, this, &cwScrapManager::updateScrapPoints);
    disconnect(scrap, &cwScrap::pointsReset, this, nullptr);
    disconnect(scrap, &cwScrap::stationAdded, this, &cwScrapManager::updateExistingScrapGeometry);
    disconnect(scrap, &cwScrap::stationPositionChanged, this, &cwScrapManager::updateScrapStations);
    disconnect(scrap, &cwScrap::stationRemoved, this, &cwScrapManager::updateScrapStation);
    disconnect(scrap, &cwScrap::stationNameChanged, this, &cwScrapManager::updateScrapStation);
    disconnect(scrap, &cwScrap::leadsInserted, this, &cwScrapManager::scrapLeadInserted);
    disconnect(scrap, &cwScrap::leadsRemoved, this, &cwScrapManager::scrapLeadRemoved);
    disconnect(scrap, &cwScrap::leadsDataChanged, this, &cwScrapManager::scrapLeadUpdated);
    disconnect(scrap, &cwScrap::viewMatrixChanged, this, nullptr);
    disconnect(scrap->viewMatrix(), &cwAbstractScrapViewMatrix::matrixChanged, this, nullptr);
}

/**
  This function will run a task that will
  1. Crop out the scrap from the note, with mimap levels
  2. Generate geometry for the scrap
  3. Morph the geometry to the station positions
  */
void cwScrapManager::updateScrapGeometry(QList<cwScrap *> scraps) {


    for(cwScrap* scrap : scraps) {
        connect(scrap, &cwScrap::destroyed,
                this, &cwScrapManager::scrapDeleted,
                Qt::UniqueConnection);

        DirtyScraps.insert(scrap);
    }

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
    if(scraps.isEmpty()) {
        return;
    }

    //Union NeedUpdate list with scraps, these are the scraps that need to be updated
    for(cwScrap* scrap : scraps) {
        cwTriangulatedData oldData = scrap->triangulationData();
        oldData.setStale(true);
        scrap->setTriangulationData(oldData);
    }

    auto run = [this]() {
        //Running
        auto dirtyScraps = cw::toList(DirtyScraps);

        if(dirtyScraps.isEmpty()) {
            return AsyncFuture::completed();
        }

        QList<cwTriangulateInData> scrapData = cw::transform(dirtyScraps, mapScrapToTriangulateInData);

        cwTriangulateTask task;
        task.setProjectFilename(Project->filename());
        task.setScrapData(scrapData);
        task.setFormatType(cwTextureUploadTask::format());
        auto allFutures = task.triangulate();

        auto combine = AsyncFuture::combine() << allFutures;

        auto finalFuture = combine.subscribe([this, dirtyScraps, allFutures]()
        {
            auto scrapDatas = cw::transform(allFutures,
                                            [](const QFuture<cwTriangulatedData>& data)
            {
                Q_ASSERT(data.resultCount() == 1);
                Q_ASSERT(data.isFinished());
                return data.result();
            });

            taskFinished(dirtyScraps, scrapDatas);
        }).future();

        FutureManagerToken.addJob({finalFuture, "Updating Scaps"});
        return finalFuture;
    };

    cwAsyncFuture::restart(&TriangulateFuture, run);
}

/**
    Extracts data from the cwScrap and puts it into a cwTriangulateInData
  */
cwTriangulateInData cwScrapManager::mapScrapToTriangulateInData(cwScrap *scrap) {
    cwTriangulateInData data;
    cwCave* cave = scrap->parentNote()->parentTrip()->parentCave();
    data.setNoteImage(scrap->parentNote()->image());
    data.setOutline(scrap->points());
    data.setStations(mapNoteStationsToTriangulateStation(scrap->stations(), cave->stationPositionLookup()));
    data.setNoteTransform(*(scrap->noteTransformation()));
    data.setViewMatrix(scrap->viewMatrix()->data()->clone());

    double dotsPerMeter = scrap->parentNote()->imageResolution()->convertTo(cwUnits::DotsPerMeter).value();
    data.setNoteImageResolution(dotsPerMeter);

    data.setLeads(scrap->leads());

    return data;
}

/**
  Extracts the note station's position and note position
  */
QList<cwTriangulateStation> cwScrapManager::mapNoteStationsToTriangulateStation(QList<cwNoteStation> noteStations,
                                                                                const cwStationPositionLookup& positionLookup) {
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
        ScrapsEntity->addScrap(scrap);
//        GLScraps->addScrapToUpdate(scrap);

        //Make sure the scrap's previously calculated data is okay.
        if((scrap->triangulationData().isStale() ||
                scrap->triangulationData().isNull() ||
                !scrapImagesOkay(scrap))
                &&
                isScrapGeometryValid(scrap))
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

//        GLScraps->removeScrap(scrap);
        ScrapsEntity->removeScrap(scrap);
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
void cwScrapManager::regenerateScrapGeometry(cwScrap* scrap)
{
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
    if(isScrapGeometryValid(scrap)) {
        regenerateScrapGeometryHelper(scrap);
    }
}

void cwScrapManager::removeScrapPoints(int begin, int end)
{
    Q_UNUSED(begin);
    Q_UNUSED(end);
    cwScrap* scrap = static_cast<cwScrap*>(sender());
    regenerateScrapGeometryHelper(scrap); //Don't check it the scrap is valid, because user may have remove last points
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
    if(isScrapGeometryValid(scrap)) {
        updateExistingScrapGeometryHelper(scrap);
    }
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
void cwScrapManager::taskFinished(const QList<cwScrap*>& scrapsToUpdate,
                                  const QList<cwTriangulatedData>& scrapDataset) {
    if(scrapDataset.isEmpty()) {
        //No scrap data udpated...
        return;
    }

    //Clear all the scraps that need to be update, because we are updating now
    foreach(cwScrap* scrap, DirtyScraps) {
        disconnect(scrap, &cwScrap::destroyed, this, &cwScrapManager::scrapDeleted);
    }
    DirtyScraps.clear();

    //Make sure there's the same amount of data
    if(scrapsToUpdate.size() != scrapDataset.size()) {
        qDebug() << "Scrap size mismatch" << LOCATION;
        return;
    }

    //All the images to remove (replacing the previously calculated or invalid images)
    QList<cwImage> imagesToRemove;

    //Get all the valid scraps
    QList<cwScrap*> validScraps;
    QList<cwTriangulatedData> validScrapTriangleDataset;
    for(int i = 0; i < scrapsToUpdate.size(); i++) {
        cwScrap* scrap = scrapsToUpdate.at(i);
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
        imagesToRemove.append(image);
    }

    auto filename = Project->filename();
    auto removeFuture = QtConcurrent::run([filename, imagesToRemove](){
        cwImageDatabase imageDatabase(filename);
        for(auto image : imagesToRemove) {
            imageDatabase.removeImages(image.ids());
        }
    });

    FutureManagerToken.addJob(cwFuture(removeFuture, "Removing Old Images"));

    for(int i = 0; i < validScraps.size(); i++) {
        cwScrap* scrap = validScraps.at(i);

        cwTriangulatedData triangleData = validScrapTriangleDataset.at(i);
        Q_ASSERT(!triangleData.isStale());

        //Remove the ownership requirements, so it doesn't ge delete from database
        triangleData.croppedImagePtr()->take();

        scrap->setTriangulationData(triangleData);
//            GLScraps->addScrapToUpdate(scrap);
    }
}

/**
  \brief Sets the gl scraps for the manager
  */
void cwScrapManager::setGLScraps(cwGLScraps *glScraps)
{
    GLScraps = glScraps;
    GLScraps->setProject(Project);
    GLScraps->setFutureManagerToken(FutureManagerToken);
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
        updateScrapGeometry(cw::toList(DirtyScraps));
    }
}

void cwScrapManager::waitForFinish()
{
    cwAsyncFuture::waitForFinished(TriangulateFuture);
}
