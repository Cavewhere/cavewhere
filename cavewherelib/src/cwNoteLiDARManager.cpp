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

// Ours
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

void cwNoteLiDARManager::setRenderGLTF(cwRenderGLTF *renderGltf)
{
    m_renderGltf = renderGltf;
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
            connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
            markDirty(note);
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
    in.setModelMatrix(note->modelMatrix());

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
    qDebug() << "Mark as dirty:" << note;

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
    qDebug() << "Run batch!";

    if (m_dirtyNotes.isEmpty()) {
        return;
    }

    // Snapshot and clear “deleted” guard
    NotePtrList notes;
    notes.reserve(m_dirtyNotes.size());
    for (cwNoteLiDAR* n : std::as_const(m_dirtyNotes)) {
        if (n != nullptr && n->parentTrip() != nullptr && n->parentCave() != nullptr) {
            notes.append(n);
        }
    }
    if (notes.isEmpty()) {
        return;
    }

    // Prepare inputs
    auto inputs = cw::transform(notes, [this](const cwNoteLiDAR* note) {
        return mapNoteToInData(note, m_project);
    });

    // Fire task
    auto future = cwTriangulateLiDARTask::triangulate(inputs);

    m_futureManagerToken.addJob({ QFuture<void>(future), "Triangulating LiDAR notes" });

    m_renderGltf->setGltf(future);

    // Wrap in restarter so subsequent calls coalesce
    m_restarter.restart([this, notes, future]() mutable {
        // Observe and fan-out completion, then signal

        return AsyncFuture::observe(future)
            .context(this,
                     [this, notes, future]() {
                         // Remove processed from dirty, clear deleted set entries
                         for (cwNoteLiDAR* n : notes) {
                             disconnect(n, nullptr, this, nullptr); // remove destroyed handler uniqueness if desired
                         }
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
        qDebug() << "Connecting model" << model;

        connect(model, &QAbstractItemModel::rowsInserted,
                this, &cwNoteLiDARManager::liDARRowsInserted, Qt::UniqueConnection);
        connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                this, &cwNoteLiDARManager::liDARRowsAboutToBeRemoved, Qt::UniqueConnection);

        // Existing notes
        for (cwNoteLiDAR* note : notesFromModel(model)) {
            qDebug() << "Connecting note" << note;
            connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
            markDirty(note);
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
        disconnect(model, &QAbstractItemModel::rowsInserted,
                   this, &cwNoteLiDARManager::liDARRowsInserted);
        disconnect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                   this, &cwNoteLiDARManager::liDARRowsAboutToBeRemoved);

        for (cwNoteLiDAR* note : notesFromModel(model)) {
            disconnect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed);
            m_dirtyNotes.remove(note);
        }
    }
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
