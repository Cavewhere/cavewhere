#pragma once

/**************************************************************************
**
**    Copyright (C) 2025 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

#ifndef CWNOTELIDARMANAGER_H
#define CWNOTELIDARMANAGER_H

// Qt
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QModelIndex>
#include <QQmlEngine>
#include <QImage>
#include <QMetaObject>

// Fwd decls
class cwProject;
class cwRegionTreeModel;
class cwLinePlotManager;
class cwCavingRegion;
class cwCave;
class cwTrip;
class cwSurveyNoteLiDARModel;
class cwNoteLiDAR;
// Ours
#include "cwGlobals.h"
#include "cwFutureManagerToken.h"
#include "asyncfuture.h"
#include "cwTriangulateLiDARInData.h"
#include "cwRenderTexturedItems.h"
#include "cwConnectionRegistry.h"
#include "cwKeywordItemRegistry.h"
#include "cwUpdatable.h"
class cwKeywordItemModel;
class cwRenderTexturedItemsVisibilityGroup;


/**
 * @brief Manages LiDAR note triangulation in response to model changes and centerline updates.
 *
 * - Walks the region tree to discover trips, then subscribes to each trip's cwSurveyNoteLiDARModel.
 * - Tracks dirty cwNoteLiDAR objects and batches them through cwTriangulateLiDARTask.
 * - Exposes slots to trigger recomputation when the cave centerline (station positions or survey network) changes.
 */
class CAVEWHERE_LIB_EXPORT cwNoteLiDARManager : public QObject, public cwUpdatableBase
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteLiDARManager)

public:
    explicit cwNoteLiDARManager(QObject* parent = nullptr);
    ~cwNoteLiDARManager();

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setFutureManagerToken(cwFutureManagerToken token);
    void setKeywordItemModel(cwKeywordItemModel* keywordItemModel);

    void setRender(cwRenderTexturedItems* renderGltf);
    void setKeepRenderGeometry(bool keepGeometry);
    bool keepRenderGeometry() const;

    // Call when a cave’s centerline changed (station positions, network, etc.)
    Q_INVOKABLE void updateLiDARForCave(cwCave* cave);
    Q_INVOKABLE void updateLiDARForTrip(cwTrip* trip);

    // Blocks until outstanding batch finishes
    void waitForFinish();

    Q_INVOKABLE void saveIcon(const QImage& icon, cwNoteLiDAR* note);

    QVector<uint32_t> renderItemIds(cwNoteLiDAR* note) const;

    // Map helpers, exposed for testing, shouldn't be used directly
    static cwTriangulateLiDARInData mapNoteToInData(const cwNoteLiDAR* note, const cwProject *project);

signals:
    void updateStateChanged();
    // Emitted after a successful triangulation batch completes
    void liDARNotesUpdated(const QList<cwNoteLiDAR*>& notes);

private slots:
    // Region tree wiring
    void handleRegionReset();
    void regionRowsInserted(QModelIndex parent, int begin, int end);
    void regionRowsAboutToBeRemoved(QModelIndex parent, int begin, int end);

    // Note model wiring
    void liDARRowsInserted(const QModelIndex& parent, int begin, int end);
    void liDARRowsAboutToBeRemoved(const QModelIndex& parent, int begin, int end);

    // Centerline triggers (if bound via line plot or elsewhere)
    void stationPositionsChangedForCave(cwCave* cave);

    // Bookkeeping
    void noteDestroyed(QObject* noteObj);

private:
    cwUpdatable::State doUpdateState() const override;
    QFuture<void> doRun() override;

    // Batch scheduling
    void markDirty(cwNoteLiDAR* note);
    // Notifies the coordinator of dirtiness and, when standalone, recomputes now.
    void notifyDirty();
    QFuture<void> runBatch();

    // Announces a state change, if there was one, after a note left the dirty set.
    // Removing the last runnable dirty note takes the pipeline Dirty -> Clean, and
    // the coordinator's staleness aggregate has to hear about that like any other
    // transition — otherwise the footer keeps offering to compute notes that are
    // gone.
    //
    // Every caller announces once, after its removals, never per note: the
    // coordinator may run the pipeline in response, and a dirty set only half
    // walked still holds notes on their way out.
    //
    // Only for removals whose notes are still alive — the model's
    // rowsAboutToBeRemoved and the explicit trip disconnect. Taking the "before"
    // state means reading every note in the dirty set, so a path reached from a
    // note's own destroyed() has to decide without it (see noteDestroyed()).
    void announceStateChange(cwUpdatable::State previousState);

    // Leaves Working: finishes the run's future and emits updateStateChanged.
    // That future is what whoever asked for the batch waits on, so every
    // completion path has to reach here.
    void finishBatch();

    // Trip wiring
    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    void connectNote(cwNoteLiDAR* note);
    void updateIconFromCache(cwNoteLiDAR* note);
    void addKeywordItemForNote(cwNoteLiDAR* note);
    void removeKeywordItemForNote(cwNoteLiDAR* note);

    // Utilities
    static QList<cwNoteLiDAR*> notesFromModel(cwSurveyNoteLiDARModel* model);

private:
    QPointer<cwRegionTreeModel> m_regionModel;
    cwLinePlotManager* m_linePlotManager = nullptr;
    cwProject* m_project = nullptr;

    cwFutureManagerToken m_futureManagerToken;
    AsyncFuture::Restarter<void> m_restarter;

    // One note waiting to be triangulated, with the trip and cave it hung from
    // when it was marked. Both are cached so isRunnable() reads only this entry:
    // a trip destroys its own members before ~QObject deletes the notes it owns,
    // so walking note -> trip -> cave to answer a question about the note reads
    // a dead trip (#637). The cave itself is still read through, since whether
    // its centerline is solved changes after the note is marked.
    struct DirtyNote {
        cwNoteLiDAR* note = nullptr;
        QPointer<cwTrip> trip;
        QPointer<cwCave> cave;

        bool isRunnable() const;
        bool operator==(const DirtyNote&) const = default;
    };

    QHash<cwNoteLiDAR*, DirtyNote> m_dirtyNotes;
    QSet<cwNoteLiDAR*> m_deletedNotes;

    // The staleness half of updateState() (see cwUpdatable::State), mirroring the
    // line plot and scraps: set when a note is (re)dirtied and cleared at
    // dispatch, so a fresh edit mid-run reports Dirty. The busy half is the run's
    // future, held by cwUpdatableBase.
    bool m_workPending = false;
    QHash<cwNoteLiDAR*, QVector<uint32_t>> m_noteToRender;
    cwKeywordItemRegistry<cwNoteLiDAR*> m_keywordRegistry;

    QPointer<cwRenderTexturedItems> m_render;

    bool m_keepRenderGeometry = false;
    QMetaObject::Connection m_pathReadyConnection;

    //Couples duplicate-connection checking with connect/disconnect (receiver = this)
    cwConnectionRegistry m_connectionRegistry;
};

#endif // CWNOTELIDARMANAGER_H
