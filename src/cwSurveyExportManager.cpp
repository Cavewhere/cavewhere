/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyExportManager.h"
#include "cwSurvexExporterTripTask.h"
#include "cwSurvexExporterCaveTask.h"
#include "cwSurvexExporterRegionTask.h"
#include "cwCompassExporterCaveTask.h"
#include "cwChipdataExporterCaveTask.h"
#include "cwDebug.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwGlobals.h"


//Qt includes
#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QItemSelectionModel>
#include <QDebug>
#include <QThread>

cwSurveyExportManager::cwSurveyExportManager(QObject *parent) :
    QObject(parent),
    ExportThread(new QThread(this))
{
}

/**
    Destructor
  */
cwSurveyExportManager::~cwSurveyExportManager() {
    ExportThread->exit();
    ExportThread->wait();
}

/**
  \brief Exports the survex region to filename
  */
void cwSurveyExportManager::exportSurvexRegion(QString filename) {
    if(filename.isEmpty()) { return; }
    if(cavingRegion() == nullptr) {
        qDebug() << "Caving region is null! this is a bug" << LOCATION;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    cwSurvexExporterRegionTask* exportTask = new cwSurvexExporterRegionTask();
    exportTask->setOutputFile(filename);
    exportTask->setData(*cavingRegion());
    connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
    connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
    exportTask->setThread(ExportThread);
    exportTask->start();
}

/**
  \brief Exports the currently selected cave to a file
  */
void cwSurveyExportManager::exportSurvexCave(QString filename) {
    if(filename.isEmpty()) { return; }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    cwCave* cave = currentCave();

    if(cave != nullptr) {
        cwSurvexExporterCaveTask* exportTask = new cwSurvexExporterCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*cave);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}

/**
  \brief Exports the currently selected trip to filename
  */
void cwSurveyExportManager::exportSurvexTrip(QString filename) {
    if(filename.isEmpty()) { return; }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    cwTrip* trip = currentTrip();
    if(trip != nullptr) {
        cwSurvexExporterTripTask* exportTask = new cwSurvexExporterTripTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*trip);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}

/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToCompass(QString filename) {
    Q_UNUSED(filename);
    if(filename.isEmpty()) { return; }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "dat");

    cwCave* cave = currentCave();
    if(cave != nullptr) {
        cwCompassExportCaveTask* exportTask = new cwCompassExportCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*cave);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}


/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToChipdata(QString filename) {
    Q_UNUSED(filename);
    if(filename.isEmpty()) { return; }
    filename = cwGlobals::convertFromURL(filename);

    cwCave* cave = currentCave();
    if(cave != nullptr) {
        cwChipdataExportCaveTask* exportTask = new cwChipdataExportCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(*cave);
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->setThread(ExportThread);
        exportTask->start();
    }
}


/**
  \brief Called when the export task has completed
  */
void cwSurveyExportManager::exporterFinished() {
    if(sender()) {
        sender()->deleteLater();
    }
}

/**
  Updates the current menu action with the current value of the
  selection model
  */
void cwSurveyExportManager::updateActions() {
    emit updateMenu();
}

/**
Gets currentCaveName
*/
QString cwSurveyExportManager::currentCaveName() const {
    return currentCave() != nullptr ? currentCave()->name() : QString();
}

/**
Gets currentTripName
*/
QString cwSurveyExportManager::currentTripName() const {
    return currentTrip() != nullptr ? currentTrip()->name() : QString();
}

/**
  Gets the current selected cave
  */
cwCave *cwSurveyExportManager::currentCave() const
{
    return cave();
}

/**
  Gets the current selected trip
  */
cwTrip* cwSurveyExportManager::currentTrip() const {
    return trip();
}

/**
* @brief cwSurveyExportManager::trip
* @return The current trip that is assigned to the survey export manager
*/
cwTrip* cwSurveyExportManager::trip() const {
    return Trip;
}

/**
* @brief cwSurveyExportManager::setTrip
* @param Sets the current trip for the survey export manager. This will also call setCave() with
* the trip's parentCave
*/
void cwSurveyExportManager::setTrip(cwTrip* trip) {
    if(Trip != trip) {
        Trip = trip;
        setCave(Trip->parentCave());
        updateActions();
        emit tripChanged();
    }
}

/**
* @brief cwSurveyExportManager::cave
* @return The current cave that is assigned to this export manager
*/
cwCave* cwSurveyExportManager::cave() const {
    return Cave;
}

/**
 * @brief cwSurveyExportManager::setCave
 * @param cave - Sets the cave for this manager. This will also call setCavingRegion() with the
 * cave's parent()
 */
void cwSurveyExportManager::setCave(cwCave* cave) {
    if(Cave != cave) {
        Cave = cave;
        Q_ASSERT(qobject_cast<cwCavingRegion*>(Cave->parent()) != nullptr);
        setCavingRegion(qobject_cast<cwCavingRegion*>(Cave->parent()));
        updateActions();
        emit caveChanged();
    }
}

/**
* @brief cwSurveyExportManager::cavingRegion
* @return - Returns the current region for the export manager
*/
cwCavingRegion* cwSurveyExportManager::cavingRegion() const {
    return CavingRegion;
}

/**
 * @brief cwSurveyExportManager::setCavingRegion
 * @param cavingRegion - Returns the caving region for this manager
 */
void cwSurveyExportManager::setCavingRegion(cwCavingRegion* cavingRegion) {
    if(CavingRegion != cavingRegion) {
        CavingRegion = cavingRegion;
        updateActions();
        emit cavingRegionChanged();
    }
}
