/**************************************************************************
**
**    Copyright (C) 2025
**    www.cavewhere.com
**
**************************************************************************/

// This header
#include "cwNoteLiDARManager.h"

// Qt
#include <QAbstractItemModel>
#include <QDebug>
#include <QDir>
#include <QImage>

// Ours
#include "cwNoteLiDARTransformation.h"
#include "cwRegionTreeModel.h"
#include "cwTriangulateLiDARTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "cwAsyncFuture.h"
#include "cwNoteLiDARTransformation.h"
#include "cwUniqueConnectionChecker.h"
#include "cwSaveLoad.h"
#include "cwImage.h"

// Async
#include "asyncfuture.h"

using NotePtrList = QList<cwNoteLiDAR*>;

cwNoteLiDARManager::cwNoteLiDARManager(QObject* parent) :
    QObject(parent),
    m_restarter(this)
{
    m_restarter.onFutureChanged([this]() {
        m_futureManagerToken.addJob({ m_restarter.future(), "Triangulating LiDAR notes" });
    });
}

cwNoteLiDARManager::~cwNoteLiDARManager()
{
    m_restarter.future().cancel();
    waitForFinish();
}

void cwNoteLiDARManager::setProject(cwProject* project)
{
    m_project = project;
}

void cwNoteLiDARManager::setRegionTreeModel(cwRegionTreeModel* regionTreeModel)
{
    if (m_regionModel == regionTreeModel) {
        return;
    }

    if (!m_regionModel.isNull()) {
        disconnect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                   this, &cwNoteLiDARManager::regionRowsInserted);
        disconnect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                   this, &cwNoteLiDARManager::regionRowsAboutToBeRemoved);
    }

    m_regionModel = regionTreeModel;

    if (!m_regionModel.isNull()) {
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                this, &cwNoteLiDARManager::regionRowsInserted);
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                this, &cwNoteLiDARManager::regionRowsAboutToBeRemoved);
        handleRegionReset();
    }
}

void cwNoteLiDARManager::setLinePlotManager(cwLinePlotManager* linePlotManager)
{
    if (m_linePlotManager == linePlotManager) {
        return;
    }

    if (m_linePlotManager != nullptr) {
        // If you have a specific LiDAR-centered signal, connect/disconnect it here.
        // Example (adjust to your actual signal):
        // disconnect(m_linePlotManager, &cwLinePlotManager::stationPositionInLiDARChanged,
        //            this, &cwNoteLiDARManager::stationPositionsChangedForCave);
        disconnect(m_linePlotManager, nullptr, this, nullptr);
    }

    m_linePlotManager = linePlotManager;

    if (m_linePlotManager != nullptr) {
        // Example hookup (commented until such a signal exists):
        connect(m_linePlotManager, &cwLinePlotManager::stationPositionInTripsChanged,
                this, [this](const QList<cwTrip*>& trips) {

                    for(const auto trip : trips) {
                        const auto notes = trip->notesLiDAR()->notes();
                        for(const auto note : notes) {
                            Q_ASSERT(dynamic_cast<cwNoteLiDAR*>(note));
                            markDirty(static_cast<cwNoteLiDAR*>(note));
                        }
                    }

                    runIfNeeded();
        });
    }
}

void cwNoteLiDARManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwNoteLiDARManager::setRender(cwRenderTexturedItems *render)
{
    m_render = render;
}

void cwNoteLiDARManager::setKeepRenderGeometry(bool keepGeometry)
{
    m_keepRenderGeometry = keepGeometry;
}

bool cwNoteLiDARManager::keepRenderGeometry() const
{
    return m_keepRenderGeometry;
}

bool cwNoteLiDARManager::automaticUpdate() const
{
    return m_automaticUpdate;
}

void cwNoteLiDARManager::setAutomaticUpdate(bool automaticUpdate)
{
    if (m_automaticUpdate == automaticUpdate) {
        return;
    }
    m_automaticUpdate = automaticUpdate;
    emit automaticUpdateChanged();
    runIfNeeded();
}

void cwNoteLiDARManager::updateAllLiDAR()
{
    if (m_regionModel.isNull() || m_regionModel->cavingRegion() == nullptr) {
        return;
    }

    const NotePtrList all = collectAllNotes(m_regionModel);
    for (cwNoteLiDAR* note : all) {
        markDirty(note);
    }
    runIfNeeded();
}

void cwNoteLiDARManager::updateLiDARForCave(cwCave* cave)
{
    if (cave == nullptr) {
        return;
    }
    for (cwTrip* trip : cave->trips()) {
        updateLiDARForTrip(trip);
    }
}

void cwNoteLiDARManager::updateLiDARForTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }
    if (auto* model = trip->notesLiDAR()) {
        for (cwNoteLiDAR* note : notesFromModel(model)) {
            markDirty(note);
        }
    }
    runIfNeeded();
}

void cwNoteLiDARManager::waitForFinish()
{
    cwAsyncFuture::waitForFinished(m_restarter.future());
}

void cwNoteLiDARManager::saveIcon(const QImage& icon, cwNoteLiDAR* note)
{
    if (note == nullptr || m_project == nullptr || icon.isNull()) {
        return;
    }

    // if (!note->iconImagePath().isEmpty() || m_iconCaptureInFlight.contains(note)) {
    //     return;
    // }

    const QDir destinationDir = cwSaveLoad::dir(note);
    QPointer<cwNoteLiDAR> notePtr(note);
    // cwNoteLiDAR* const rawNote = note;

    // m_iconCaptureInFlight.insert(note);

    m_project->saveImage(
        icon,
        destinationDir,
        [this, notePtr](const cwImage& savedImage) {
            // m_iconCaptureInFlight.remove(rawNote);

            if (notePtr.isNull()) {
                return;
            }


            if (savedImage.isValid()) {
                qDebug() << "Save image valid:" << savedImage << savedImage.isValid();
                notePtr->setIconImagePath(savedImage.path());
            }
        });
}

// ---------------------- Region model wiring ----------------------

void cwNoteLiDARManager::handleRegionReset()
{
    if (m_regionModel.isNull() || m_regionModel->cavingRegion() == nullptr) {
        return;
    }

    // Connect all existing trips
    for (cwCave* cave : m_regionModel->cavingRegion()->caves()) {
        for (cwTrip* trip : cave->trips()) {
            connectTrip(trip);
        }
    }
}

void cwNoteLiDARManager::regionRowsInserted(QModelIndex parent, int begin, int end)
{
    if (!m_regionModel) {
        return;
    }

    if (!m_regionModel->isCave(parent) && !m_regionModel->isRegion(parent)) {
        // We only care about trips being added; notes are handled via trip->notesLiDAR()
        // The RegionTreeModel API provides isTrip(index) checks when iterating children.
    }

    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = m_regionModel->index(i, 0, parent);
        if (m_regionModel->isTrip(idx)) {
            if (auto* trip = m_regionModel->trip(idx)) {
                connectTrip(trip);
            }
        }
    }
}

void cwNoteLiDARManager::regionRowsAboutToBeRemoved(QModelIndex parent, int begin, int end)
{
    if (!m_regionModel) {
        return;
    }

    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = m_regionModel->index(i, 0, parent);
        if (m_regionModel->isTrip(idx)) {
            if (auto* trip = m_regionModel->trip(idx)) {
                disconnectTrip(trip);
            }
        }
    }
}

// ---------------------- Notes model wiring ----------------------

void cwNoteLiDARManager::liDARRowsInserted(const QModelIndex& parent, int begin, int end)
{
    Q_UNUSED(parent);

    auto* model = qobject_cast<cwSurveyNoteLiDARModel*>(sender());
    if (model == nullptr) {
        return;
    }

    // Mark newly added notes dirty and watch destruction
    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = model->index(i, 0);
        if (!idx.isValid()) {
            continue;
        }
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
            connectNote(note);
        }
    }

    runIfNeeded();
}

void cwNoteLiDARManager::liDARRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end)
{
    Q_UNUSED(parent);

    auto* model = qobject_cast<cwSurveyNoteLiDARModel*>(sender());
    if (model == nullptr) {
        return;
    }

    for (int i = begin; i <= end; i++) {
        const QModelIndex idx = model->index(i, 0);
        if (!idx.isValid()) {
            continue;
        }
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
            m_deletedNotes.insert(note);
            m_dirtyNotes.remove(note);

            disconnect(note->noteTransformation(), nullptr, this, nullptr);
            disconnect(note, nullptr, this, nullptr);
        }
    }
}

// ---------------------- Centerline trigger ----------------------

void cwNoteLiDARManager::stationPositionsChangedForCave(cwCave* cave)
{
    updateLiDARForCave(cave);
}

// ---------------------- Bookkeeping ----------------------

void cwNoteLiDARManager::noteDestroyed(QObject* noteObj)
{
    if (auto* note = static_cast<cwNoteLiDAR*>(noteObj)) {
        m_deletedNotes.insert(note);
        m_dirtyNotes.remove(note);
        // m_iconCaptureInFlight.remove(note);
    }
}

// ---------------------- Mapping and batch ----------------------

cwTriangulateLiDARInData cwNoteLiDARManager::mapNoteToInData(const cwNoteLiDAR* note, const cwProject* project)
{
    cwTriangulateLiDARInData in;

    if (note == nullptr) {
        return in;
    }

    const cwTrip* trip = note->parentTrip();
    const cwCave* cave = trip ? trip->parentCave() : nullptr;

    // Note stations
    in.setNoteStations(note->stations());

    // Model matrix for lidar
    in.setModelMatrix(note->noteTransformation()->matrix());

    // Station lookup and network from cave when available
    if (cave != nullptr) {
        in.setStationLookup(cave->stationPositionLookup());
        in.setSurveyNetwork(cave->network());
    }

    // GLTF path (if the note exposes one via filename())
    in.setGltfFilename(project->absolutePath(note->filename()));


    return in;
}

void cwNoteLiDARManager::markDirty(cwNoteLiDAR* note)
{
    if (note == nullptr) {
        return;
    }

    // connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
    m_dirtyNotes.insert(note);
}

void cwNoteLiDARManager::runIfNeeded()
{
    if (!m_automaticUpdate) {
        return;
    }
    runBatch();
}

void cwNoteLiDARManager::runBatch()
{
    if (m_dirtyNotes.isEmpty()) {
        return;
    }

    // Snapshot and clear “deleted” guard
    NotePtrList notes;
    notes.reserve(m_dirtyNotes.size());
    for (cwNoteLiDAR* note : std::as_const(m_dirtyNotes)) {
        if (note != nullptr
            && note->parentTrip() != nullptr
            && note->parentCave() != nullptr
            && note->rowCount() > 0 //Make sure there's stations
            )
        {
            notes.append(note);
        }
    }

    if (notes.isEmpty()) {        
        return;
    }

    // Prepare inputs
    auto inputs = cw::transform(notes, [this](const cwNoteLiDAR* note) {
        return mapNoteToInData(note, m_project);
    });

    // Wrap in restarter so subsequent calls coalesce
    m_restarter.restart([this, notes, inputs]() {
        auto future = cwTriangulateLiDARTask::triangulate(inputs);

        return AsyncFuture::observe(future)
            .context(this,
                     [this, notes, future]() {
                         Q_ASSERT(notes.size() == future.resultCount());

                         //Update the rendering scene
                         for(int i = 0; i < future.resultCount(); i++) {
                             auto note = notes.at(i);
                             if(m_deletedNotes.contains(note)) {
                                 //Note deleted, just skip the result
                                 continue;
                             }

                             auto result = future.resultAt(i);
                             if(result.hasError()) {
                                 qWarning() << "Warning: Note triangle at i:" << i << result.errorMessage();
                                 continue;
                             }

                             QVector<cwRenderTexturedItems::Item> items = future.resultAt(i).value();

                             if (m_keepRenderGeometry) {
                                 for (auto& item : items) {
                                     item.storeGeometry = true;
                                 }
                             }

                             auto addItems = [this, note](const QVector<cwRenderTexturedItems::Item>& items) {
                                 QVector<uint32_t> newIds = cw::transform(items, [this](const cwRenderTexturedItems::Item& item) {
                                     return m_render->addItem(item);
                                 });

                                 m_noteToRender[note] = newIds;
                             };

                             if(m_noteToRender.contains(note)) {
                                 //Update the existing note
                                 auto renderIds = m_noteToRender.value(note);

                                 //For now just remove all the old ids
                                 //not very efficient
                                 for(auto id : renderIds) {
                                     m_render->removeItem(id);
                                 }

                                 addItems(items);
                             } else {
                                 addItems(items);
                             }
                         }

                         // Remove processed from dirty, clear deleted set entries
                         for (cwNoteLiDAR* n : notes) {
                             m_dirtyNotes.remove(n);
                         }
                         for (cwNoteLiDAR* d : std::as_const(m_deletedNotes)) {
                             m_dirtyNotes.remove(d);
                         }
                         m_deletedNotes.clear();

                         emit liDARNotesUpdated(notes);
                     }).future();
    });
}

// ---------------------- Trip wiring helpers ----------------------

void cwNoteLiDARManager::connectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    if (auto* model = trip->notesLiDAR()) {
        if(!m_connectionChecker.add(model)) {
            return;
        }


        connect(model, &QAbstractItemModel::rowsInserted,
                this, &cwNoteLiDARManager::liDARRowsInserted);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                this, &cwNoteLiDARManager::liDARRowsAboutToBeRemoved);

        // Existing notes
        const auto notes = notesFromModel(model);
        for (cwNoteLiDAR* note : notes) {
            connectNote(note);
        }

        runIfNeeded();
    }
}

void cwNoteLiDARManager::disconnectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    if (auto* model = trip->notesLiDAR()) {
        m_connectionChecker.remove(model);
        disconnect(model, &QAbstractItemModel::rowsInserted,
                   this, &cwNoteLiDARManager::liDARRowsInserted);
        disconnect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                   this, &cwNoteLiDARManager::liDARRowsAboutToBeRemoved);

        for (cwNoteLiDAR* note : notesFromModel(model)) {
            m_connectionChecker.remove(note);

            disconnect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed);
            disconnect(note->noteTransformation(), nullptr, this, nullptr);
            disconnect(note, nullptr, this, nullptr);
            m_dirtyNotes.remove(note);
        }
    }
}

void cwNoteLiDARManager::connectNote(cwNoteLiDAR *note)
{
    if(!m_connectionChecker.add(note)) {
        return;
    }

    auto handleNoteChange = [note, this]() {
        markDirty(note);
        runIfNeeded();
    };

    connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
    connect(note->noteTransformation(), &cwNoteLiDARTransformation::matrixChanged, this, handleNoteChange);
    bool connected = connect(note, &cwNoteLiDAR::dataChanged, this, [handleNoteChange](QModelIndex, QModelIndex, QVector<int>) { handleNoteChange(); });
    connect(note, &cwNoteLiDAR::rowsInserted, this, handleNoteChange);
    connect(note, &cwNoteLiDAR::rowsRemoved, this, handleNoteChange);

    markDirty(note);
}

// ---------------------- Utilities ----------------------

QList<cwTrip*> cwNoteLiDARManager::allTrips(cwRegionTreeModel* regionModel)
{
    QList<cwTrip*> out;
    if (regionModel == nullptr || regionModel->cavingRegion() == nullptr) {
        return out;
    }
    for (cwCave* cave : regionModel->cavingRegion()->caves()) {
        for (cwTrip* trip : cave->trips()) {
            out.append(trip);
        }
    }
    return out;
}

NotePtrList cwNoteLiDARManager::notesFromModel(cwSurveyNoteLiDARModel* model)
{
    NotePtrList out;
    if (model == nullptr) {
        return out;
    }

    const int n = model->rowCount();
    out.reserve(n);

    for (int i = 0; i < n; i++) {
        const QModelIndex idx = model->index(i, 0);
        if (!idx.isValid()) {
            continue;
        }
        QObject* obj = model->data(idx, cwSurveyNoteModelBase::NoteObjectRole).value<QObject*>();
        if (auto* note = qobject_cast<cwNoteLiDAR*>(obj)) {
            out.append(note);
        }
    }

    return out;
}

NotePtrList cwNoteLiDARManager::collectAllNotes(cwRegionTreeModel* regionModel)
{
    NotePtrList out;
    for (cwTrip* trip : allTrips(regionModel)) {
        if (auto* model = trip->notesLiDAR()) {
            out.append(notesFromModel(model));
        }
    }
    return out;
}

QVector<uint32_t> cwNoteLiDARManager::renderItemIds(cwNoteLiDAR* note) const
{
    if (note == nullptr) {
        return {};
    }
    return m_noteToRender.value(note);
}
