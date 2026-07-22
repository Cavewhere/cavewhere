/**************************************************************************
**
**    Copyright (C) 2025
**    www.cavewhere.com
**
**************************************************************************/

// This header
#include "cwNoteLiDARManager.h"
#include "cwRestarterTracking.h"

// Qt
#include <QAbstractItemModel>
#include <QDebug>
#include <QDir>
#include <QImage>
#include <QFileInfo>
#include <QFile>
#include <QBuffer>
#include <QUrl>
#include <QtGlobal>

// Ours
#include "cwNoteLiDARTransformation.h"
#include "cwNoteTranformation.h"
#include "cwRegionTreeModel.h"
#include "cwTriangulateLiDARTask.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwGridConvergence.h"
#include "cwTrip.h"
#include "cwSurveyNoteLiDARModel.h"
#include "cwNoteLiDAR.h"
#include "cwLinePlotManager.h"
#include "cwProject.h"
#include "asyncfuture.h"
#include "cwDiskCacher.h"
#include "cwCacheImageProvider.h"
#include "cwKeywordItemModel.h"
#include "cwKeywordItem.h"
#include "cwRenderTexturedItemsVisibilityGroup.h"
#include "xxhash.h"

// Async
#include "asyncfuture.h"

// Std
#include <algorithm>

using NotePtrList = QList<cwNoteLiDAR*>;

namespace {

// A dirty note is only worth triangulating once it is attached to a cave/trip,
// has stations, and its cave centerline is solved. Shared by updateState() and
// the run path.
bool isRunnableNote(const cwNoteLiDAR* note)
{
    return note != nullptr
        && note->parentTrip() != nullptr
        && note->parentCave() != nullptr
        && note->rowCount() > 0
        && !note->parentCave()->stationPositionLookup().positions().isEmpty();
}

cwDiskCacher::Key iconCacheKey(const cwProject* project, const cwNoteLiDAR* note)
{
    cwDiskCacher::Key key;

    QString path = (project && note) ? project->absolutePath(note, note->filename()) : QString();
    if (!path.isEmpty()) {
        QFileInfo fi(path);
        key.path = QDir(fi.path());
        QString baseName = fi.fileName();
        if (baseName.isEmpty()) {
            baseName = fi.completeBaseName();
        }
    key.id = baseName + QStringLiteral(".icon.png");
    } else {
        key.path = QDir(QStringLiteral("lidar-icons"));
        QString name = note ? note->name() : QString();
        if (name.isEmpty()) {
            name = QStringLiteral("note-%1").arg(reinterpret_cast<quintptr>(note), 0, 16);
        }
        key.id = name + QStringLiteral(".icon.png");
    }

    return key;
}

QString sourceChecksum(const cwProject* project, const cwNoteLiDAR* note)
{
    if (project == nullptr || note == nullptr) {
        return {};
    }

    const QString absolutePath = project->absolutePath(note, note->filename());
    if (absolutePath.isEmpty()) {
        return {};
    }

    QFile file(absolutePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    XXH3_state_t* state = XXH3_createState();
    if (!state) {
        return {};
    }
    XXH3_64bits_reset(state);

    while (!file.atEnd()) {
        QByteArray chunk = file.read(1024 * 1024);
        if (chunk.isEmpty()) {
            break;
        }
        XXH3_64bits_update(state, chunk.constData(), static_cast<size_t>(chunk.size()));
    }

    const XXH64_hash_t hashValue = XXH3_64bits_digest(state);
    XXH3_freeState(state);

    return QString::number(static_cast<qulonglong>(hashValue), 16);
}

QString cacheUrlForKey(const cwDiskCacher::Key& key)
{
    const QString encoded = cwCacheImageProvider::encodeKey(key);
    const QByteArray escaped = QUrl::toPercentEncoding(encoded);
    return QStringLiteral("image://%1/%2").arg(cwCacheImageProvider::name(), QString::fromLatin1(escaped));
}

}

cwNoteLiDARManager::cwNoteLiDARManager(QObject* parent) :
    QObject(parent),
    m_restarter(this),
    m_connectionRegistry(this)
{
    cwTrackRestarter(m_futureManagerToken, m_restarter, QStringLiteral("Triangulating LiDAR notes"));
}

cwNoteLiDARManager::~cwNoteLiDARManager()
{
    m_restarter.future().cancel();
    waitForFinish();
}

void cwNoteLiDARManager::setProject(cwProject* project)
{
    if (m_project == project) {
        return;
    }

    if (m_pathReadyConnection) {
        disconnect(m_pathReadyConnection);
    }

    m_project = project;

    if (m_project != nullptr) {
        m_pathReadyConnection = connect(m_project, &cwProject::objectPathReady, this, [this](QObject* object) {
            if (object == nullptr) {
                return;
            }

            if (auto* note = qobject_cast<cwNoteLiDAR*>(object)) {
                updateIconFromCache(note);
                markDirty(note);
                notifyDirty();
                return;
            }

            if (auto* trip = qobject_cast<cwTrip*>(object)) {
                updateLiDARForTrip(trip);
                return;
            }

            if (auto* cave = qobject_cast<cwCave*>(object)) {
                updateLiDARForCave(cave);
            }
        });
    }
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
        disconnect(m_regionModel.data(), &cwRegionTreeModel::modelReset,
                   this, &cwNoteLiDARManager::handleRegionReset);
    }

    m_regionModel = regionTreeModel;

    if (!m_regionModel.isNull()) {
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsInserted,
                this, &cwNoteLiDARManager::regionRowsInserted);
        connect(m_regionModel.data(), &cwRegionTreeModel::rowsAboutToBeRemoved,
                this, &cwNoteLiDARManager::regionRowsAboutToBeRemoved);
        // A model reset (project load / git checkout) replaces every row without
        // per-row signals, so re-walk the reloaded trips. Mirrors cwScrapManager.
        connect(m_regionModel.data(), &cwRegionTreeModel::modelReset,
                this, &cwNoteLiDARManager::handleRegionReset);
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

                    notifyDirty();
        });
    }
}

void cwNoteLiDARManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwNoteLiDARManager::setKeywordItemModel(cwKeywordItemModel *keywordItemModel)
{
    if(m_keywordRegistry.model() == keywordItemModel) {
        return;
    }

    m_keywordRegistry.setModel(keywordItemModel);

    if(keywordItemModel == nullptr) {
        return;
    }

    for(auto note : m_noteToRender.keys()) {
        addKeywordItemForNote(note);
    }
}

void cwNoteLiDARManager::setRender(cwRenderTexturedItems *render)
{
    m_render = render;

    if(m_render && m_keywordRegistry.model() != nullptr) {
        for(auto note : m_noteToRender.keys()) {
            addKeywordItemForNote(note);
        }
    }
}

void cwNoteLiDARManager::setKeepRenderGeometry(bool keepGeometry)
{
    m_keepRenderGeometry = keepGeometry;
}

bool cwNoteLiDARManager::keepRenderGeometry() const
{
    return m_keepRenderGeometry;
}

cwUpdatable::State cwNoteLiDARManager::updateState() const
{
    // Dirty takes priority over Working: a note (re)dirtied but not yet handed to
    // a batch (m_workPending) reports Dirty even while an earlier batch runs, so
    // the coordinator re-drives update() and the restarter coalesces the fresh
    // edit. Once dispatched, a running batch reports Working until it completes.
    // See cwUpdatable::State.
    const bool runnableDirty =
        std::any_of(m_dirtyNotes.begin(), m_dirtyNotes.end(), &isRunnableNote);
    if(m_workPending && runnableDirty) { return cwUpdatable::State::Dirty; }
    if(m_taskRunning)                  { return cwUpdatable::State::Working; }
    return cwUpdatable::State::Clean;
}

void cwNoteLiDARManager::update()
{
    runBatch();
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
    emit updateStateChanged();
    update();
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
    notifyDirty();
}

void cwNoteLiDARManager::waitForFinish()
{
    AsyncFuture::waitForFinished(m_restarter.future());
}

void cwNoteLiDARManager::saveIcon(const QImage& icon, cwNoteLiDAR* note)
{
    if (note == nullptr || m_project == nullptr || icon.isNull()) {
        return;
    }

    cwDiskCacher cacher(m_project ? m_project->dataRootDir() : QDir());
    auto key = iconCacheKey(m_project, note);
    // key.checksum = sourceChecksum(m_project, note);
    // if (key.checksum.isEmpty()) {
    //     return;
    // }

    QByteArray pngData;
    QBuffer buffer(&pngData);
    buffer.open(QIODevice::WriteOnly);
    if (!icon.save(&buffer, "PNG")) {
        return;
    }

    cacher.insert(key, pngData);
    note->setIconImagePath(cacheUrlForKey(key));
}

void cwNoteLiDARManager::updateIconFromCache(cwNoteLiDAR* note)
{
    if (note == nullptr || m_project == nullptr) {
        return;
    }

    cwDiskCacher::Key key = iconCacheKey(m_project, note);
    // qDebug() << "Key:" << key.path << key.id;

    //Don't checksum, it's slow
    // key.checksum = sourceChecksum(m_project, note);
    // if (key.checksum.isEmpty()) {
    //     note->setIconImagePath(QString());
    //     return;
    // }

    cwDiskCacher cacher(m_project ? m_project->dataRootDir() : QDir());
    if (cacher.hasEntry(key)) {
        note->setIconImagePath(cacheUrlForKey(key));
    } else {
        note->setIconImagePath(QString());
    }
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

    notifyDirty();
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

            removeKeywordItemForNote(note);
            m_noteToRender.remove(note);

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
        removeKeywordItemForNote(note);
        m_noteToRender.remove(note);
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
    QMatrix4x4 modelMatrix = note->noteTransformation()->matrix();
    if(trip != nullptr && trip->calibrations() != nullptr) {
        cwNoteLiDARTransformationData data = note->noteTransformation()->data();
        data.north = cwNoteTranformation::northAdjustedForDeclination(data.north,
                                                                      trip->calibrations()->declination());
        // Add back the grid convergence the store side
        // (cwNoteLiDAR::updateNoteTransformion) subtracted, so the note north
        // matches the grid-aligned plotted stations (0.0 without a projected
        // CS). See issue #628.
        const double convergence = cwGridConvergence::angleForCave(cave);
        data.north = cwNoteTranformation::northAdjustedForDeclination(data.north, -convergence);
        cwNoteLiDARTransformation adjustedTransform;
        adjustedTransform.setData(data);
        modelMatrix = adjustedTransform.matrix();
    }
    in.setModelMatrix(modelMatrix);

    // Station lookup and network from cave when available
    if (cave != nullptr) {
        in.setStationLookup(cave->stationPositionLookup());
        in.setSurveyNetwork(cave->network());
    }

    // GLTF path (if the note exposes one via filename())
    const QString path = (project && note) ? project->absolutePath(note, note->filename()) : QString();
    in.setGltfFilename(path);

    return in;
}

void cwNoteLiDARManager::markDirty(cwNoteLiDAR* note)
{
    if (note == nullptr) {
        return;
    }

    // connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
    m_dirtyNotes.insert(note);
    m_workPending = true;
}

void cwNoteLiDARManager::notifyDirty()
{
    emit updateStateChanged();
    runIfStandalone();
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
        if (isRunnableNote(note)) {
            notes.append(note);
        }
    }

    if (notes.isEmpty()) {
        return;
    }

    // Dispatching now covers the current dirty set: drop the pending marker and
    // enter Working. A note dirtied after this re-sets m_workPending (markDirty),
    // flipping back to Dirty so the restarter re-runs.
    m_workPending = false;
    m_taskRunning = true;
    emit updateStateChanged();

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

                             const QVector<uint32_t> oldIds = m_noteToRender.value(note);

                             if (oldIds.size() == items.size() && !items.isEmpty()) {
                                 // Re-triangulation from a declination or transform edit
                                 // produces the same number of items with new geometry.
                                 // Update them in place: reusing the render ids lets
                                 // cwRenderTexturedItems coalesce repeated edits onto a
                                 // stable id and skips the picker/visibility churn of
                                 // tearing every item down and re-adding it. The ids are
                                 // unchanged, so the note's keyword/visibility binding
                                 // still holds and needs no rebind.
                                 for (int itemIndex = 0; itemIndex < items.size(); ++itemIndex) {
                                     m_render->updateItem(oldIds.at(itemIndex), items.at(itemIndex));
                                 }
                             } else {
                                 // First build, or the item count changed (e.g. the note's
                                 // GLB was replaced): tear down the old items and add fresh
                                 // ones, then rebind the keyword item to the new ids.
                                 for (uint32_t id : oldIds) {
                                     m_render->removeItem(id);
                                 }

                                 const QVector<uint32_t> newIds = cw::transform(items, [this](const cwRenderTexturedItems::Item& item) {
                                     return m_render->addItem(item);
                                 });
                                 m_noteToRender[note] = newIds;

                                 addKeywordItemForNote(note);
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

                         // Batch done: leave Working (updateState reflects the
                         // notes just removed from m_dirtyNotes — Clean, or Dirty
                         // if an edit arrived mid-batch).
                         m_taskRunning = false;
                         emit updateStateChanged();
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
        const bool added = m_connectionRegistry.add(model, [this, model]{
            connect(model, &QAbstractItemModel::rowsInserted,
                    this, &cwNoteLiDARManager::liDARRowsInserted);
            connect(model, &QAbstractItemModel::rowsAboutToBeRemoved,
                    this, &cwNoteLiDARManager::liDARRowsAboutToBeRemoved);

            // Existing notes
            for (cwNoteLiDAR* note : notesFromModel(model)) {
                connectNote(note);
            }
        });

        if (added) {
            notifyDirty();
        }
    }
}

void cwNoteLiDARManager::disconnectTrip(cwTrip* trip)
{
    if (trip == nullptr) {
        return;
    }

    if (auto* model = trip->notesLiDAR()) {
        // remove() tears down the model↔this row connections wholesale (equivalent to
        // the two specific disconnects this replaced).
        m_connectionRegistry.remove(model);

        for (cwNoteLiDAR* note : notesFromModel(model)) {
            m_connectionRegistry.remove(note);
            // The note's transformation is a second source object, so it isn't covered
            // by the note's own wholesale disconnect above.
            disconnect(note->noteTransformation(), nullptr, this, nullptr);
            m_deletedNotes.insert(note);
            m_dirtyNotes.remove(note);
            removeKeywordItemForNote(note);
            m_noteToRender.remove(note);
        }
    }
}

void cwNoteLiDARManager::connectNote(cwNoteLiDAR *note)
{
    const bool added = m_connectionRegistry.add(note, [this, note]{
        auto handleNoteChange = [note, this]() {
            markDirty(note);
            notifyDirty();
        };

        connect(note, &QObject::destroyed, this, &cwNoteLiDARManager::noteDestroyed, Qt::UniqueConnection);
        connect(note->noteTransformation(), &cwNoteLiDARTransformation::matrixChanged, this, handleNoteChange);
        connect(note, &cwNoteLiDAR::dataChanged, this, [handleNoteChange](QModelIndex, QModelIndex, QVector<int>) { handleNoteChange(); });
        connect(note, &cwNoteLiDAR::rowsInserted, this, handleNoteChange);
        connect(note, &cwNoteLiDAR::rowsRemoved, this, handleNoteChange);
        connect(note, &cwNoteLiDAR::filenameChanged, this, [this, note, handleNoteChange]() {
            updateIconFromCache(note);
            handleNoteChange();
        });
    });

    if (!added) {
        return;
    }

    addKeywordItemForNote(note);
    markDirty(note);
    updateIconFromCache(note);
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

void cwNoteLiDARManager::addKeywordItemForNote(cwNoteLiDAR *note)
{
    if(!note || m_keywordRegistry.model() == nullptr || !m_render) {
        return;
    }

    auto renderIdsIter = m_noteToRender.constFind(note);
    if(renderIdsIter == m_noteToRender.constEnd() || renderIdsIter.value().isEmpty()) {
        return;
    }

    // Render ids can change across rebuilds, so always rebind from scratch.
    m_keywordRegistry.drop(note);

    const QVector<uint32_t> renderIds = renderIdsIter.value();
    m_keywordRegistry.ensure(note, [this, note, renderIds]() {
        auto keywordItem = new cwKeywordItem();
        keywordItem->keywordModel()->addExtension(note->keywordModel());

        // The visibility proxy is parented to the keyword item, so it dies with it.
        auto visibility = new cwRenderTexturedItemsVisibilityGroup(m_render, renderIds, keywordItem);
        keywordItem->setObject(visibility);
        return keywordItem;
    });
}

void cwNoteLiDARManager::removeKeywordItemForNote(cwNoteLiDAR *note)
{
    m_keywordRegistry.drop(note);
}
