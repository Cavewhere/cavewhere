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
#include "cwAsyncFuture.h"
#include "cwTriangulateLiDARInData.h"
#include "cwRenderGLTF.h"


/**
 * @brief Manages LiDAR note triangulation in response to model changes and centerline updates.
 *
 * - Walks the region tree to discover trips, then subscribes to each trip's cwSurveyNoteLiDARModel.
 * - Tracks dirty cwNoteLiDAR objects and batches them through cwTriangulateLiDARTask.
 * - Exposes slots to trigger recomputation when the cave centerline (station positions or survey network) changes.
 */
class CAVEWHERE_LIB_EXPORT cwNoteLiDARManager : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(NoteLiDARManager)

    Q_PROPERTY(bool automaticUpdate READ automaticUpdate WRITE setAutomaticUpdate NOTIFY automaticUpdateChanged)

public:
    explicit cwNoteLiDARManager(QObject* parent = nullptr);
    ~cwNoteLiDARManager();

    void setProject(cwProject* project);
    void setRegionTreeModel(cwRegionTreeModel* regionTreeModel);
    void setLinePlotManager(cwLinePlotManager* linePlotManager);
    void setFutureManagerToken(cwFutureManagerToken token);

    void setRenderGLTF(cwRenderGLTF* renderGltf);

    bool automaticUpdate() const;
    void setAutomaticUpdate(bool automaticUpdate);

    // Useful in tests or manual recompute
    Q_INVOKABLE void updateAllLiDAR();

    // Call when a cave’s centerline changed (station positions, network, etc.)
    Q_INVOKABLE void updateLiDARForCave(cwCave* cave);
    Q_INVOKABLE void updateLiDARForTrip(cwTrip* trip);

    // Blocks until outstanding batch finishes
    void waitForFinish();

signals:
    void automaticUpdateChanged();
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
    // Map helpers
    static cwTriangulateLiDARInData mapNoteToInData(const cwNoteLiDAR* note, const cwProject *project);

    // Batch scheduling
    void markDirty(cwNoteLiDAR* note);
    void runIfNeeded();
    void runBatch();

    // Trip wiring
    void connectTrip(cwTrip* trip);
    void disconnectTrip(cwTrip* trip);

    // Utilities
    static QList<cwNoteLiDAR*> collectAllNotes(cwRegionTreeModel* regionModel);
    static QList<cwNoteLiDAR*> notesFromModel(cwSurveyNoteLiDARModel* model);
    static QList<cwTrip*> allTrips(cwRegionTreeModel* regionModel);

private:
    QPointer<cwRegionTreeModel> m_regionModel;
    cwLinePlotManager* m_linePlotManager = nullptr;
    cwProject* m_project = nullptr;

    cwFutureManagerToken m_futureManagerToken;
    cwAsyncFuture::Restarter<void> m_restarter;

    QSet<cwNoteLiDAR*> m_dirtyNotes;
    QSet<cwNoteLiDAR*> m_deletedNotes;

    QPointer<cwRenderGLTF> m_renderGltf;

    bool m_automaticUpdate = true;
};

#endif // CWNOTELIDARMANAGER_H
