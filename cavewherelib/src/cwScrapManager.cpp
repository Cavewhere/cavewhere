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
#include "cwImageProvider.h"
#include "cwDiskCacher.h"
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
#include "cwSketch.h"
#include "cwPenStrokeModel.h"
#include "cwSurveyNoteSketchModel.h"
#include "cwSketchScrapOutlineDetector.h"
#include "cwSketchScrapOutline.h"
#include "cwSketchScrapRasterizer.h"
#include "cwSketchPainter.h"
#include "cwSketchManager.h"
#include "cwScale.h"
#include "cwTripLinePlotTask.h"
#include "cwStation.h"
#include "cwNoteStation.h"
#include "cwImage.h"

//Async future
#include "asyncfuture.h"

//Std includes
#include <algorithm>
#include <ranges>

namespace {
// cwImageProvider cache-key prefix used for the rasterised texture that
// backs every sketch-derived scrap. The prefix + content hash form a
// namespace within cwDiskCacher; identical rasters collide harmlessly.
inline const QString kSketchTextureCacheKeyPrefix =
    QStringLiteral("sketch-texture");
} // namespace

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
    if (Project == project) {
        return;
    }

    if (m_pathReadyConnection) {
        disconnect(m_pathReadyConnection);
    }

    Project = project;

    if (Project != nullptr) {
        m_pathReadyConnection = connect(Project, &cwProject::objectPathReady, this, [this](QObject* object) {
            if (object == nullptr) {
                return;
            }

            if (auto* note = qobject_cast<cwNote*>(object)) {
                if (!note->scraps().isEmpty()) {
                    updateScrapGeometry(note->scraps());
                }
                return;
            }

            if (auto* trip = qobject_cast<cwTrip*>(object)) {
                QList<cwScrap*> scraps;
                if (auto* notesModel = trip->notes()) {
                    for (QObject* obj : notesModel->notes()) {
                        if (auto* note = qobject_cast<cwNote*>(obj)) {
                            scraps.append(note->scraps());
                        }
                    }
                }
                if (!scraps.isEmpty()) {
                    updateScrapGeometry(scraps);
                }
                return;
            }

            if (auto* cave = qobject_cast<cwCave*>(object)) {
                QList<cwScrap*> scraps;
                for (cwTrip* trip : cave->trips()) {
                    if (trip == nullptr) {
                        continue;
                    }
                    if (auto* notesModel = trip->notes()) {
                        for (QObject* obj : notesModel->notes()) {
                            if (auto* note = qobject_cast<cwNote*>(obj)) {
                                scraps.append(note->scraps());
                            }
                        }
                    }
                }
                if (!scraps.isEmpty()) {
                    updateScrapGeometry(scraps);
                }
            }
        });
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
            disconnect(RegionModel.data(), &cwRegionTreeModel::modelReset,
                       this, &cwScrapManager::handleRegionReset);
        }

        RegionModel = regionTreeModel;

        if(!RegionModel.isNull()) {
            connect(RegionModel.data(), &cwRegionTreeModel::rowsInserted,
                    this, &cwScrapManager::inserted);
            connect(RegionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                    this, &cwScrapManager::removed);
            connect(RegionModel.data(), &cwRegionTreeModel::modelReset,
                    this, &cwScrapManager::handleRegionReset);
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

void cwScrapManager::setSketchManager(cwSketchManager *sketchManager)
{
    if(m_sketchManager == sketchManager) {
        return;
    }

    if(!m_sketchManager.isNull()) {
        disconnect(m_sketchManager.data(), &cwSketchManager::linePlotUpdated,
                   this, nullptr);
        for(auto it = m_sketchLinePlotTrip.begin(); it != m_sketchLinePlotTrip.end(); ++it) {
            if(!it.value().isNull()) {
                m_sketchManager->releaseLinePlot(it.value().data());
            }
        }
    }
    m_sketchLinePlotTrip.clear();

    m_sketchManager = sketchManager;

    if(m_sketchManager.isNull()) {
        return;
    }

    connect(m_sketchManager.data(), &cwSketchManager::linePlotUpdated,
            this, [this](cwTrip* trip) {
        if(trip == nullptr) {
            return;
        }
        for(auto it = m_sketchDerivedScraps.begin(); it != m_sketchDerivedScraps.end(); ++it) {
            cwSketch* sketch = it.key();
            if(sketch != nullptr && sketch->parentTrip() == trip) {
                updateDerivedScrapsForSketch(sketch);
            }
        }
    });

    for(auto it = m_sketchDerivedScraps.begin(); it != m_sketchDerivedScraps.end(); ++it) {
        cwSketch* sketch = it.key();
        if(sketch == nullptr) {
            continue;
        }
        cwTrip* trip = sketch->parentTrip();
        if(trip == nullptr) {
            continue;
        }
        m_sketchManager->acquireLinePlot(trip);
        m_sketchLinePlotTrip.insert(sketch, trip);
    }
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
    m_sketchScrapBoundingBox.remove(scrap);
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
                if(auto* sketchModel = trip->notesSketch()) {
                    for(QObject* obj : sketchModel->notes()) {
                        if(auto* sketch = qobject_cast<cwSketch*>(obj)) {
                            connectSketch(sketch);
                            sketchInsertedHelper(sketch);
                        }
                    }
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
    } else if(RegionModel->isNotesSketch(parent)) {
        for(int i = begin; i <= end; i++) {
            QModelIndex idx = RegionModel->index(i, 0, parent);
            if(auto* sketch = idx.data(cwRegionTreeModel::ObjectRole).value<cwSketch*>()) {
                connectSketch(sketch);
                sketchInsertedHelper(sketch);
            }
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
    } else if(RegionModel->isNotesSketch(parent)) {
        for(int i = begin; i <= end; i++) {
            QModelIndex idx = RegionModel->index(i, 0, parent);
            if(auto* sketch = idx.data(cwRegionTreeModel::ObjectRole).value<cwSketch*>()) {
                sketchRemovedHelper(sketch);
                disconnectSketch(sketch);
            }
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
    connect(scrap, &cwScrap::editingChanged, this, [this, scrap](bool editing) {
        if(!editing) {
            updateScrapGeometry({scrap});
        }
    });

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
    // disconnect(note, &cwNote::imageResolutionChanged, this, &cwScrapManager::updateScrapsWithNewNoteResolution);
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
    disconnect(scrap, &cwScrap::editingChanged, this, nullptr);
    disconnect(scrap, &cwScrap::viewMatrixChanged, this, nullptr);
    disconnect(scrap->viewMatrix(), &cwAbstractScrapViewMatrix::matrixChanged, this, nullptr);
}

void cwScrapManager::connectSketch(cwSketch* sketch)
{
    // Guard against double-connect: handleRegionReset can run after rows have
    // already been wired via inserted(), and modelReset can re-walk the tree.
    if(sketch == nullptr || m_sketchDerivedScraps.contains(sketch)) {
        return;
    }

    connect(sketch, &cwSketch::strokesReset,
            this, [this, sketch]() { updateDerivedScrapsForSketch(sketch); });
    connect(sketch, &cwSketch::strokeEnded,
            this, [this, sketch]() { updateDerivedScrapsForSketch(sketch); });

    // cwSurveyNoteModelBase::clearNotes uses beginResetModel and never fires
    // rowsRemoved — without a destroyed handler the tracking map would retain
    // dangling pointers once the owning model deleteLater's the sketch.
    connect(sketch, &QObject::destroyed, this, [this](QObject* obj) {
        auto* sk = static_cast<cwSketch*>(obj);
        m_sketchDerivedScraps.remove(sk);
        m_sketchDebugEntries.remove(sk);
        releaseSketchLinePlot(sk);
    });

    if(auto* model = sketch->strokeModel()) {
        connect(model, &QAbstractItemModel::rowsRemoved,
                this, [this, sketch]() { updateDerivedScrapsForSketch(sketch); });
    }

    if(!m_sketchManager.isNull()) {
        if(cwTrip* trip = sketch->parentTrip()) {
            m_sketchManager->acquireLinePlot(trip);
            m_sketchLinePlotTrip.insert(sketch, trip);
        }
    }
}

void cwScrapManager::disconnectSketch(cwSketch* sketch)
{
    if(sketch == nullptr) {
        return;
    }
    disconnect(sketch, nullptr, this, nullptr);
    if(auto* model = sketch->strokeModel()) {
        disconnect(model, nullptr, this, nullptr);
    }
}

void cwScrapManager::sketchInsertedHelper(cwSketch* sketch)
{
    if(sketch == nullptr) {
        return;
    }
    m_sketchDerivedScraps.insert(sketch, {});
    // On load, strokes are deserialized before the region tree raises
    // rowsInserted, so the sketch's own strokesReset/strokeEnded already
    // fired with no listener attached. Run the detector now so loaded
    // projects materialize their derived scraps without waiting for an edit.
    updateDerivedScrapsForSketch(sketch);
}

void cwScrapManager::sketchRemovedHelper(cwSketch* sketch)
{
    if(sketch == nullptr) {
        return;
    }
    auto it = m_sketchDerivedScraps.find(sketch);
    if(it == m_sketchDerivedScraps.end()) {
        return;
    }

    for(cwScrap* scrap : std::as_const(it.value())) {
        m_sketchScrapBoundingBox.remove(scrap);
        detachScrap(scrap);
        scrap->deleteLater();
    }
    m_sketchDerivedScraps.erase(it);
    if(m_sketchDebugEntries.remove(sketch) > 0) {
        emit sketchDebugEntriesChanged(sketch);
    }

    releaseSketchLinePlot(sketch);
}

void cwScrapManager::releaseSketchLinePlot(cwSketch* sketch)
{
    auto it = m_sketchLinePlotTrip.find(sketch);
    if(it == m_sketchLinePlotTrip.end()) {
        return;
    }
    if(!m_sketchManager.isNull() && !it.value().isNull()) {
        m_sketchManager->releaseLinePlot(it.value().data());
    }
    m_sketchLinePlotTrip.erase(it);
}

namespace {

// Detection thresholds for outline-forming strokes. 1 m snaps near-closed
// strokes into closed polygons — generous enough that hand-drawn wall loops
// at typical paper scales (1:250) close without perfect endpoint alignment;
// 5 mm Douglas-Peucker keeps pen detail without bloating the CDT input.
// Both are placeholders until the plan's stroke-width-relative heuristic lands.
constexpr double kSketchCloseThresholdMeters    = 1.0;
constexpr double kSketchSimplifyToleranceMeters = 0.005;

QPointF normalizePointToBox(QPointF p, const QRectF& box)
{
    const double w = box.width();
    const double h = box.height();
    return {
        w > 0.0 ? (p.x() - box.x()) / w : 0.0,
        h > 0.0 ? (p.y() - box.y()) / h : 0.0
    };
}

QVector<QPointF> normalizePolygonToBoundingBox(const QPolygonF& polygon, const QRectF& box)
{
    QVector<QPointF> out;
    out.reserve(polygon.size());
    for(const QPointF& p : polygon) {
        out.append(normalizePointToBox(p, box));
    }
    return out;
}

// Build note stations for a sketch-derived outline from the trip's line
// plot components. A station qualifies if its trip-local position lands
// inside the outline's bounding box expanded by a margin of the larger
// dimension — the margin keeps the morph well-conditioned for outlines
// that hug just inside a wall with stations outside. Loop-broken
// duplicates collapse to their original name so the global
// cwStationPositionLookup resolves them.
//
// Result is sorted by canonical name so repeated calls with the same
// station set produce equal QLists — QHash iteration order is not stable
// across inserts, so unsorted output would spuriously flag "changed"
// against a prior snapshot.
// Shared filter for sketch-scrap station selection: keeps stations whose
// trip-local position falls within a bbox inflated by max(width,height),
// deduplicated by canonical name and sorted so repeated calls produce
// equal QVectors.
QVector<cwScrapManager::SketchScrapDebugStation> filteredStationsForOutline(
    const QList<cwTripLinePlotTask::TripComponent>& components,
    const QRectF& outlineBoundingBox)
{
    QVector<cwScrapManager::SketchScrapDebugStation> out;
    if(!outlineBoundingBox.isValid()) {
        return out;
    }

    const double margin = std::max(outlineBoundingBox.width(),
                                   outlineBoundingBox.height());
    const QRectF filterBox = outlineBoundingBox.adjusted(-margin, -margin,
                                                          margin, margin);

    QSet<QString> seenKeys;

    for(const auto& component : components) {
        const auto positions = component.positions.positions();
        for(auto it = positions.constBegin(); it != positions.constEnd(); ++it) {
            const QVector3D pos = it.value();
            const QPointF tripLocal(pos.x(), pos.y());
            if(!filterBox.contains(tripLocal)) {
                continue;
            }

            QString name = it.key();
            const auto remapIt = component.renameRemap.constFind(name);
            if(remapIt != component.renameRemap.constEnd()) {
                name = remapIt.value();
            }

            const QString key = cwStation::canonicalKey(name);
            // A loop-break pair collapses to the same original name; keep
            // the first occurrence (morph treats them as one anchor).
            if(seenKeys.contains(key)) {
                continue;
            }
            seenKeys.insert(key);

            out.append({name, tripLocal});
        }
    }

    std::sort(out.begin(), out.end(),
              [](const cwScrapManager::SketchScrapDebugStation& a,
                 const cwScrapManager::SketchScrapDebugStation& b) {
                  return cwStation::canonicalKey(a.name)
                       < cwStation::canonicalKey(b.name);
              });
    return out;
}

QList<cwNoteStation> toNoteStations(
    const QVector<cwScrapManager::SketchScrapDebugStation>& filtered,
    const QRectF& outlineBoundingBox)
{
    QList<cwNoteStation> out;
    out.reserve(filtered.size());
    for(const auto& s : filtered) {
        cwNoteStation noteStation;
        noteStation.setName(s.name);
        noteStation.setPositionOnNote(normalizePointToBox(s.tripLocalPos, outlineBoundingBox));
        out.append(noteStation);
    }
    return out;
}

} // namespace

void cwScrapManager::updateDerivedScrapsForSketch(cwSketch* sketch)
{
    if(sketch == nullptr) {
        return;
    }
    auto trackedIt = m_sketchDerivedScraps.find(sketch);
    if(trackedIt == m_sketchDerivedScraps.end()) {
        return;
    }

    // Outset the detected outline by half the widest wall / scrap-outline
    // stroke so the rasterized texture and triangulated mesh include the
    // full pen thickness instead of clipping at the stroke centerline.
    const double paperScale = sketch->mapScale() ? sketch->mapScale()->scale() : 0.0;
    double maxWidthPixels = 0.0;
    for (const auto &s : sketch->strokes()) {
        if (s.kind == cwPenStroke::Wall || s.kind == cwPenStroke::ScrapOutline) {
            maxWidthPixels = std::max(maxWidthPixels, s.width);
        }
    }
    const double ppm = cwSketchPainter::pixelsPerMeterFromPaperScale(
        paperScale, cwSketchScrapRasterizer::kTargetDPI);
    const double outsetMeters = (ppm > 0.0 && maxWidthPixels > 0.0)
        ? (0.5 * maxWidthPixels) / ppm
        : 0.0;

    const auto outlines = cwSketchScrapOutlineDetector::detect(
        sketch->strokes(),
        kSketchCloseThresholdMeters,
        kSketchSimplifyToleranceMeters,
        outsetMeters);

    QList<cwTripLinePlotTask::TripComponent> components;
    if(!m_sketchManager.isNull()) {
        if(cwTrip* trip = sketch->parentTrip()) {
            components = m_sketchManager->latestLinePlot(trip);
        }
    }
    const bool haveStationSource = !m_sketchManager.isNull();

    QHash<QUuid, cwScrap*>& tracked = trackedIt.value();
    QSet<QUuid> seen;
    QList<cwScrap*> dirtyScraps;
    QVector<SketchScrapDebugEntry> debugEntries;
    debugEntries.reserve(outlines.size());

    for(const auto& outline : outlines) {
        const QRectF outlineBox = outline.tripLocalPolygon.boundingRect();
        const QVector<SketchScrapDebugStation> filteredStations = haveStationSource
            ? filteredStationsForOutline(components, outlineBox)
            : QVector<SketchScrapDebugStation>();
        const QList<cwNoteStation> noteStations = toNoteStations(filteredStations, outlineBox);

        // Empty-station guard: morph needs at least one (local,global) pair,
        // so outlines with no nearby stations are dropped before they reach
        // the triangulator. When no station source is wired (unit tests),
        // scraps are still created so the 4b no-anchor path keeps working.
        if(haveStationSource && noteStations.isEmpty()) {
            continue;
        }

        SketchScrapDebugEntry debugEntry;
        debugEntry.sourceStrokeId = outline.sourceStrokeId;
        debugEntry.tripLocalPolygon = outline.tripLocalPolygon;
        debugEntry.stations = filteredStations;
        debugEntries.append(std::move(debugEntry));

        seen.insert(outline.sourceStrokeId);
        const QVector<QPointF> normalized =
            normalizePolygonToBoundingBox(outline.tripLocalPolygon, outlineBox);

        auto it = tracked.find(outline.sourceStrokeId);
        if(it == tracked.end()) {
            auto* scrap = new cwScrap(sketch);
            scrap->setParentSketch(sketch);
            scrap->setPoints(normalized);
            scrap->setStations(noteStations);
            tracked.insert(outline.sourceStrokeId, scrap);
            m_sketchScrapBoundingBox.insert(scrap, outlineBox);
            connectScrap(scrap);
            attachScrap(scrap);
            dirtyScraps.append(scrap);
        } else {
            cwScrap* scrap = it.value();
            const bool pointsChanged = scrap->points() != normalized;
            const bool stationsChanged = scrap->stations() != noteStations;
            const QRectF previousBox = m_sketchScrapBoundingBox.value(scrap);
            const bool bboxChanged = previousBox != outlineBox;
            if(pointsChanged) {
                scrap->setPoints(normalized);
            }
            if(stationsChanged) {
                scrap->setStations(noteStations);
            }
            if(bboxChanged) {
                m_sketchScrapBoundingBox.insert(scrap, outlineBox);
            }
            // bbox changes re-trigger triangulation so the rasterized texture
            // tracks the outline even when the normalized outline points are
            // byte-identical (same shape, shifted or scaled in trip-local).
            if(pointsChanged || stationsChanged || bboxChanged) {
                dirtyScraps.append(scrap);
            }
        }
    }

    for(auto it = tracked.begin(); it != tracked.end(); ) {
        if(!seen.contains(it.key())) {
            m_sketchScrapBoundingBox.remove(it.value());
            detachScrap(it.value());
            it.value()->deleteLater();
            it = tracked.erase(it);
        } else {
            ++it;
        }
    }

    // Any change to the sketch (new stroke, removed stroke, moved point)
    // calls into this function, so re-rasterize every surviving tracked
    // scrap unconditionally. Coarser than an overlap check but much simpler,
    // and correct: the rasterizer always paints the full stroke set.
    QSet<cwScrap*> dirtyScrapSet(dirtyScraps.cbegin(), dirtyScraps.cend());
    for(auto it = tracked.begin(); it != tracked.end(); ++it) {
        cwScrap* scrap = it.value();
        if(!dirtyScrapSet.contains(scrap)) {
            dirtyScraps.append(scrap);
            dirtyScrapSet.insert(scrap);
        }
    }

    if(!dirtyScraps.isEmpty()) {
        updateScrapGeometry(dirtyScraps);
    }

    const auto previousDebug = m_sketchDebugEntries.value(sketch);
    if(debugEntries.isEmpty()) {
        m_sketchDebugEntries.remove(sketch);
    } else {
        m_sketchDebugEntries.insert(sketch, debugEntries);
    }
    if(previousDebug != debugEntries) {
        emit sketchDebugEntriesChanged(sketch);
    }

    emit sketchDerivedScrapsUpdated(sketch);
}

void cwScrapManager::attachScrap(cwScrap* scrap)
{
    if(scrap == nullptr || m_renderScraps.isNull()) {
        return;
    }
    cwRenderMaterialState state;
    state.cullMode = cwRenderMaterialState::CullMode::None;
    m_scrapToRenderId.insert(scrap,
                             m_renderScraps->addItem({cwGeometry(), QImage(), state}));
    addKeywordItemForScrap(scrap);
}

void cwScrapManager::detachScrap(cwScrap* scrap)
{
    if(scrap == nullptr) {
        return;
    }
    addToDeletedScraps(scrap);
    disconnectScrap(scrap);
    removeKeywordItemForScrap(scrap);
    if(m_scrapToRenderId.contains(scrap)) {
        auto id = m_scrapToRenderId.take(scrap);
        if(!m_renderScraps.isNull()) {
            m_renderScraps->removeItem(id);
        }
    }
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

    updateScrapGeometryHelper(scraps);
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
    task.setDataRootDir(Project->dataRootDir());
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

    if(!automaticUpdate()) {
        return;
    }

    const bool hasRunnableScrap = std::any_of(DirtyScraps.begin(), DirtyScraps.end(), [](const cwScrap* scrap) {
        return scrap != nullptr && !scrap->editing() && scrap->parentCave() != nullptr;
    });

    if(!hasRunnableScrap) {
        return;
    }


    auto run = [this]() {

        //Running
        auto dirtyScrapsRange =
            cw::toList(DirtyScraps)
            | std::views::filter([](const cwScrap* scrap) {
                return scrap != nullptr && !scrap->editing() && scrap->parentCave() != nullptr;
            });

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
    data.setOutline(scrap->points());
    data.setViewMatrix(scrap->viewMatrix()->data()->clone());
    data.setLeads(scrap->leads());
    data.setMorphingSettings(m_warpingSettings->data());

    if(scrap->parentKind() == cwScrap::SketchParent) {
        // The rasterizer produces a texture whose "paper" is cave-meters
        // (pixelsPerMeter is trip-local), and the outline / station
        // positionOnNote are normalized to the same outline bounding box —
        // so paper-meters == cave-meters for this scrap, which is exactly
        // what cwNoteTransformationData's default (1:1, north=0) gives us.
        cwSketch* sketch = scrap->parentSketch();
        const QRectF bbox = m_sketchScrapBoundingBox.value(scrap);

        QImage rasterized;
        double pixelsPerMeter = 1.0;
        if(sketch != nullptr && !bbox.isEmpty()) {
            rasterized = cwSketchScrapRasterizer::rasterize(sketch, bbox);
            if(!rasterized.isNull() && bbox.width() > 0.0) {
                pixelsPerMeter = double(rasterized.width()) / bbox.width();
            }
        }

        cwImage noteImage;
        if(!rasterized.isNull() && Project != nullptr) {
            // cwTriangulateTask reads its input texture off disk via
            // cwImageProvider, so publish the rasterized QImage into the
            // project's shared image cache and hand out the resulting path.
            // The content hash is folded into the cache key, so identical
            // rasters land on the same on-disk file and collide harmlessly.
            const QString pathHint = QStringLiteral("sketch_scrap_%1.png")
                .arg(sketch->id().toString(QUuid::WithoutBraces));
            const quint64 hash = cwImageProvider::imageHash(rasterized);
            const cwDiskCacher::Key key = cwImageProvider::imageCacheKey(
                pathHint, kSketchTextureCacheKeyPrefix, hash);
            const cwDiskCacher::Key written = cwImageProvider::addToImageCache(
                Project->dataRootDir(), rasterized, key);
            cwDiskCacher cacher(Project->dataRootDir());
            noteImage.setOriginalSize(rasterized.size());
            noteImage.setPath(cacher.filePath(written));
        } else {
            noteImage.setOriginalSize(rasterized.isNull() ? QSize(1, 1)
                                                          : rasterized.size());
        }

        data.setNoteImage(noteImage);
        data.setNoteImageResolution(pixelsPerMeter);
        data.setNoteTransform({});
        data.setNoteStation(scrap->stations());
        if(auto* cave = scrap->parentCave()) {
            data.setStationLookup(cave->stationPositionLookup());
            data.setSurveyNetwork(cave->network());
        }
        return data;
    }

    cwCave* cave = scrap->parentNote()->parentTrip()->parentCave();

    cwImage noteImage = scrap->parentNote()->image();
    if (Project) {
        data.setNoteImage(Project->absolutePathNoteImage(scrap->parentNote()));
    }
    data.setNoteStation(scrap->stations());
    data.setStationLookup(cave->stationPositionLookup());
    data.setSurveyNetwork(cave->network());
    data.setNoteTransform(scrap->noteTransformAdjustedDeclination());

    double dotsPerMeter = scrap->parentNote()->imageResolution()->convertTo(cwUnits::DotsPerMeter).value;
    data.setNoteImageResolution(dotsPerMeter);

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

        connectScrap(scrap);
        attachScrap(scrap);

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
        detachScrap(parentNote->scrap(i));
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
