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
#include "cwImageResolution.h"
#include "cwLinePlotManager.h"
#include "cwTaskManagerModel.h"
#include "cwRegionTreeModel.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwRenderTexturedItemVisibility.h"
#include "cwImageDatabase.h"
#include "asyncfuture.h"
#include "cwAbstractScrapViewMatrix.h"
#include "cwConcurrent.h"
#include "cwTriangulateWarping.h"
#include "cwTriangulateWarpingSettings.h"
#include "cwSaveLoad.h"

//Async future
#include "asyncfuture.h"

//Std includes
#include <ranges>

cwScrapManager::cwScrapManager(QObject *parent) :
    QObject(parent),
    LinePlotManager(nullptr),
    Project(nullptr),
    TriangulateRestarter(this),
    m_renderScraps(nullptr),
    AutomaticUpdate(true),
    m_warpingSettings(new cwTriangulateWarping(this))
{
    TriangulateRestarter.onFutureChanged([this](){
        FutureManagerToken.addJob({TriangulateRestarter.future(), "Updating Scaps"});
    });

    auto notifyWarpingChanged = [this]() {
        updateAllScraps();
    };

    connect(m_warpingSettings, &cwTriangulateWarping::gridResolutionMetersChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::shotInterpolationSpacingMetersChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::maxClosestStationsChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::smoothingRadiusMetersChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::useShotInterpolationSpacingChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::useMaxClosestStationsChanged,
            this, notifyWarpingChanged);
    connect(m_warpingSettings, &cwTriangulateWarping::useSmoothingRadiusChanged,
            this, notifyWarpingChanged);

    m_warpingSettingsStore = std::make_unique<cwTriangulateWarpingSettings>(m_warpingSettings, this);
}

cwScrapManager::~cwScrapManager()
{
    TriangulateRestarter.future().cancel();
    waitForFinish();
}

/**
  \brief Sets the project for the scrap manager
  */
void cwScrapManager::setProject(cwProject *project) {
    Project = project;
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

void cwScrapManager::setFutureManagerToken(cwFutureManagerToken token)
{
    FutureManagerToken = token;
    // if(m_renderScraps) {
    //     m_renderScraps->setFutureManagerToken(token);
    // }
}

void cwScrapManager::setKeywordItemModel(cwKeywordItemModel *keywordItemModel)
{
    if(m_keywordItemModel == keywordItemModel) {
        return;
    }

    if(m_keywordItemModel) {
        for(auto iter = m_scrapKeywordEntries.begin(); iter != m_scrapKeywordEntries.end(); ++iter) {
            if(iter.value().item) {
                m_keywordItemModel->removeItem(iter.value().item);
                iter.value().item->deleteLater();
            }
            if(iter.value().visibility) {
                iter.value().visibility->deleteLater();
            }
        }
        m_scrapKeywordEntries.clear();
    }

    m_keywordItemModel = keywordItemModel;

    if(!m_keywordItemModel) {
        return;
    }

    for(auto scrap : m_scrapToRenderId.keys()) {
        addKeywordItemForScrap(scrap);
    }
}

/**
  This function is for testing

  This will gather all the scraps from all the caves, and trips, and notes and regenerate
  all there geometry
  */
void cwScrapManager::updateAllScraps() {
    if(!RegionModel) {
        return;
    }

    //Get all the scraps for the whole region
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
// bool cwScrapManager::scrapImagesOkay(cwScrap *scrap)
// {
//     auto image = scrap->triangulationData().croppedImage();
//     return cwImageDatabase(Project->filename()).mipmapsValid(image,
//                                                              false);
// }

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
                // qDebug() << "Connect note:" << note;
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

    for(cwScrap* scrap : std::as_const(scraps)) {
        connect(scrap, &cwScrap::destroyed,
                this, &cwScrapManager::scrapDeleted,
                Qt::UniqueConnection);

        DirtyScraps.insert(scrap);
    }

    if(AutomaticUpdate) {
        updateScrapGeometryHelper(scraps);
    }
}

QList<cwScrapManager::TriangulatedScrapResult> cwScrapManager::triangulateScraps(const QList<cwScrap *> &scraps) const
{
    QList<TriangulatedScrapResult> results;

    if(scraps.isEmpty() || Project == nullptr) {
        return results;
    }

    QList<cwTriangulateInData> scrapData = cw::transform(scraps, [this](cwScrap* scrap) {
        return mapScrapToTriangulateInData(scrap);
    });

    if(scrapData.isEmpty()) {
        return results;
    }

    cwTriangulateTask task;
    task.setProjectFilename(Project->filename());
    task.setScrapData(scrapData);
    task.setFormatType(cwTextureUploadTask::format());
    auto triangulatedFutures = task.triangulate();

    if(triangulatedFutures.isEmpty()) {
        return results;
    }

    const int count = qMin(scraps.size(), triangulatedFutures.size());
    results.reserve(count);

    for(int i = 0; i < count; i++) {
        TriangulatedScrapResult entry;
        entry.scrap = scraps.at(i);
        entry.data = triangulatedFutures.at(i);
        results.append(entry);
    }

    return results;
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
    // for(cwScrap* scrap : scraps) {
    //     cwTriangulatedData oldData = scrap->triangulationData();
    //     oldData.setStale(true);
    //     scrap->setTriangulationData(oldData);
    // }


    auto run = [this]() {

        //Running
        auto dirtyScrapsRange =
            cw::toList(DirtyScraps)
            | std::views::filter([](const cwScrap* scrap) { return scrap->parentCave() != nullptr; });

        QList<cwScrap*> dirtyScraps(dirtyScrapsRange.begin(), dirtyScrapsRange.end());

        if(dirtyScraps.isEmpty()) {
            return AsyncFuture::completed();
        }

        auto triangulationResults = triangulateScraps(dirtyScraps);

        if(triangulationResults.isEmpty()) {
            return AsyncFuture::completed();
        }

        QList<QFuture<cwTriangulatedData>> futures;
        futures.reserve(triangulationResults.size());
        for(const auto& result : std::as_const(triangulationResults)) {
            futures.append(result.data);
        }

        auto combine = AsyncFuture::combine() << futures;

        auto finalFuture = combine.context(this, [this, triangulationResults]()
        {
            QList<cwScrap*> scrapsToUpdate;
            QList<cwTriangulatedData> scrapDataset;

            scrapsToUpdate.reserve(triangulationResults.size());
            scrapDataset.reserve(triangulationResults.size());

            for(const auto& result : triangulationResults) {
                if(result.scrap == nullptr) {
                    continue;
                }

                cwTriangulatedData data;
                const auto& future = result.data;

                if(future.isFinished() && future.resultCount() == 1) {
                    data = future.result();
                }

                scrapsToUpdate.append(result.scrap);
                scrapDataset.append(data);
            }

            taskFinished(scrapsToUpdate, scrapDataset);
        }).future();

        return finalFuture;
    };

    TriangulateRestarter.restart(run);
}

/**
    Extracts data from the cwScrap and puts it into a cwTriangulateInData
  */
cwTriangulateInData cwScrapManager::mapScrapToTriangulateInData(cwScrap *scrap) const {
    cwTriangulateInData data;
    cwCave* cave = scrap->parentNote()->parentTrip()->parentCave();

    cwImage noteImage = scrap->parentNote()->image();
    data.setNoteImage(cwSaveLoad::absolutePathNoteImage(scrap->parentNote()));
    data.setOutline(scrap->points());
    data.setNoteStation(scrap->stations());
    data.setStationLookup(cave->stationPositionLookup());
    data.setSurveyNetwork(cave->network());
    data.setStations(mapNoteStationsToTriangulateStation(scrap->stations(), cave->stationPositionLookup()));
    data.setNoteTransform(scrap->noteTransformation()->data());
    data.setViewMatrix(scrap->viewMatrix()->data()->clone());

    double dotsPerMeter = scrap->parentNote()->imageResolution()->convertTo(cwUnits::DotsPerMeter).value;
    data.setNoteImageResolution(dotsPerMeter);

    data.setLeads(scrap->leads());
    data.setMorphingSettings(m_warpingSettings->data());

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
            station.setNotePosition(QVector3D(noteStation.positionOnNote()));
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

        cwRenderMaterialState state;
        state.cullMode = cwRenderMaterialState::CullMode::None;

        //Add the scrap data that's already in it
        m_scrapToRenderId.insert(scrap,
                                 m_renderScraps->addItem({cwGeometry(),
                                                          QImage(),
                                                          state}));
        addKeywordItemForScrap(scrap);

        if(isScrapGeometryValid(scrap)) {
            scrapsToUpdate.append(scrap);
        }

        //Pull data from data cache, make sure the checksum is good too

        // //Make sure the scrap's previously calculated data is okay.
        // if((scrap->triangulationData().isStale() ||
        //         scrap->triangulationData().isNull()
        //         &&
        //         isScrapGeometryValid(scrap))
        // {
        //     //Isn't okay, we need to recalculate it
        //     scrapsToUpdate.append(scrap);
        // } else if(scrap->triangulationData().croppedImageData().isNull()
        //            && scrap->triangulationData().croppedImage().isValid()) {
        //     //Load the image data from disk, async
        //     cwTextureUploadTask uploadTask;
        //     uploadTask.setImage(scrap->triangulationData().croppedImage());
        //     uploadTask.setProjectFilename(Project->filename());
        //     uploadTask.setType(cwTextureUploadTask::OpenGL_RGBA);
        //     auto future = uploadTask.mipmaps();
        //     FutureManagerToken.addJob(cwFuture(QFuture<void>(future), "Updating Texture"));

        //     QPointer<cwScrap> weakPtrScrap = scrap;

        //     AsyncFuture::observe(future).subscribe([weakPtrScrap, future, this](){
        //         if(weakPtrScrap) {
        //             auto triangleData = weakPtrScrap->triangulationData();
        //             if(triangleData.croppedImageData().isNull()) {
        //                 triangleData.setCroppedImageData(future.result());
        //                 weakPtrScrap->setTriangulationData(triangleData);
        //                 m_renderScraps->addScrapToUpdate(weakPtrScrap);
        //             }
        //         }
        //     });
        // }
    }

    //Update all the scrap geometry for the scrap
    updateScrapGeometry(scrapsToUpdate);
}

void cwScrapManager::addKeywordItemForScrap(cwScrap *scrap)
{
    if(!m_keywordItemModel || !m_renderScraps || !scrap) {
        return;
    }

    if(m_scrapKeywordEntries.contains(scrap)) {
        return;
    }

    auto renderIdIter = m_scrapToRenderId.constFind(scrap);
    if(renderIdIter == m_scrapToRenderId.constEnd()) {
        return;
    }

    auto keywordItem = new cwKeywordItem();
    keywordItem->keywordModel()->addExtension(scrap->keywordModel());

    auto visibility = new cwRenderTexturedItemVisibility(m_renderScraps, renderIdIter.value(), keywordItem);
    keywordItem->setObject(visibility);
    m_keywordItemModel->addItem(keywordItem);

    ScrapKeywordEntry entry;
    entry.item = keywordItem;
    entry.visibility = visibility;
    m_scrapKeywordEntries.insert(scrap, entry);
}

void cwScrapManager::removeKeywordItemForScrap(cwScrap *scrap)
{
    auto iter = m_scrapKeywordEntries.find(scrap);
    if(iter == m_scrapKeywordEntries.end()) {
        return;
    }

    if(m_keywordItemModel && iter.value().item) {
        m_keywordItemModel->removeItem(iter.value().item);
    }

    if(iter.value().item) {
        iter.value().item->deleteLater();
    }

    if(iter.value().visibility) {
        iter.value().visibility->deleteLater();
    }

    m_scrapKeywordEntries.erase(iter);
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
        removeKeywordItemForScrap(scrap);

        if(m_scrapToRenderId.contains(scrap)) {
            auto id = m_scrapToRenderId.take(scrap);
            m_renderScraps->removeItem(id);
        }
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

    // //Removed all cropped image data
    // foreach(cwScrap* scrap, validScraps) {
    //     cwImage image = scrap->triangulationData().croppedImage();
    //     imagesToRemove.append(image);
    // }

    // auto filename = Project->filename();
    // auto removeFuture = cwConcurrent::run([filename, imagesToRemove](){
    //     cwImageDatabase imageDatabase(filename);
    //     for(const auto& image : imagesToRemove) {
    //         imageDatabase.removeImages(image.ids());
    //     }
    // });

    // FutureManagerToken.addJob(cwFuture(removeFuture, "Removing Old Images"));

    for(int i = 0; i < validScraps.size(); i++) {
        cwScrap* scrap = validScraps.at(i);

        cwTriangulatedData triangleData = validScrapTriangleDataset.at(i);
        // Q_ASSERT(!triangleData.isStale());

        //Remove the ownership requirements, so it doesn't get delete from database
        triangleData.croppedImagePtr()->take();

        // scrap->setTriangulationData(triangleData);
        scrap->setLeadPositions(triangleData.leadPoints());

        Q_ASSERT(m_scrapToRenderId.contains(scrap));
        auto id = m_scrapToRenderId.value(scrap);
        m_renderScraps->updateGeometry(id, triangleData.scrapGeometry());
        m_renderScraps->updateTexture(id, triangleData.croppedImageData().image);
    }
}

/**
  \brief Sets the gl scraps for the manager
  */
void cwScrapManager::setRenderScraps(cwRenderTexturedItems *scraps)
{
    m_renderScraps = scraps;
    if(!m_renderScraps) {
        return;
    }

    for(auto scrap : m_scrapToRenderId.keys()) {
        addKeywordItemForScrap(scrap);
    }
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
    AsyncFuture::waitForFinished(TriangulateRestarter.future());
}

QList<cwScrap*> cwScrapManager::dirtyScraps() const
{
    return DirtyScraps.values();
}
