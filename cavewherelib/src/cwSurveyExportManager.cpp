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
#include "cwSurvexExporterRegion.h"
#include "cwCompassExporterCaveTask.h"
#include "cwChipdataExporterCaveTask.h"
#include "cwConcurrent.h"
#include "cwDebug.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwExternalCenterline.h"
#include "cwGlobals.h"
#include "asyncfuture.h"


//Qt includes
#include <QFileDialog>
#include <QMessageBox>
#include <QAction>
#include <QMenu>
#include <QItemSelectionModel>
#include <QDebug>

namespace {

// User-facing reason surfaced via exportDisabledReason and qWarning()ed by the
// programmatic-caller guard inside each export function. Wording locked by
// the master plan §7.5: it must point the user at the project tree where the
// real .svx / .dat / .mak / .wpj / .srv files live.
const QString kExportDisabledReason = QStringLiteral(
    "Cannot export — this project contains external centerline attachments. "
    "Use your original .svx / .dat / .mak / .wpj / .srv files (in each cave "
    "or trip's external-centerline/ subdir inside the project) to share.");

} // namespace

cwSurveyExportManager::cwSurveyExportManager(QObject *parent) :
    QObject(parent)
{
}

/**
    Destructor
  */
cwSurveyExportManager::~cwSurveyExportManager() {
}

/**
  \brief Exports the survex region to filename
  */
void cwSurveyExportManager::exportSurvexRegion(QString filename) {
    if(filename.isEmpty()) { return; }
    if(cavingRegion() == nullptr) {
        qWarning() << "Caving region is null! this is a bug" << LOCATION;
        return;
    }
    if(!m_canExport) {
        qWarning() << m_exportDisabledReason;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    const cwCavingRegionData regionData = cavingRegion()->data();
    auto future = cwConcurrent::run([regionData, filename]() {
        return cwSurvexExporterRegion::exportRegion(regionData, filename);
    });

    AsyncFuture::observe(future).context(this, [filename](Monad::ResultBase result) {
        if (result.hasError()) {
            qDebug() << "Survex region export failed for" << filename
                     << ":" << result.errorMessage();
        }
    });
}

/**
  \brief Exports the currently selected cave to a file
  */
void cwSurveyExportManager::exportSurvexCave(QString filename) {
    if(filename.isEmpty()) { return; }
    if(!m_canExport) {
        qWarning() << m_exportDisabledReason;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    cwCave* cave = currentCave();

    if(cave != nullptr) {
        cwSurvexExporterCaveTask* exportTask = new cwSurvexExporterCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(cave->data());
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->start();
    }
}

/**
  \brief Exports the currently selected trip to filename
  */
void cwSurveyExportManager::exportSurvexTrip(QString filename) {
    if(filename.isEmpty()) { return; }
    if(!m_canExport) {
        qWarning() << m_exportDisabledReason;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "svx");

    cwTrip* trip = currentTrip();
    if(trip != nullptr) {
        cwSurvexExporterTripTask* exportTask = new cwSurvexExporterTripTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(trip->data());
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->start();
    }
}

/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToCompass(QString filename) {
    Q_UNUSED(filename);
    if(filename.isEmpty()) { return; }
    if(!m_canExport) {
        qWarning() << m_exportDisabledReason;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);
    filename = cwGlobals::addExtension(filename, "dat");

    cwCave* cave = currentCave();
    if(cave != nullptr) {
        cwCompassExportCaveTask* exportTask = new cwCompassExportCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(cave->data());
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
        exportTask->start();
    }
}


/**
  Exports the currently select cave to Compass
  */
void cwSurveyExportManager::exportCaveToChipdata(QString filename) {
    Q_UNUSED(filename);
    if(filename.isEmpty()) { return; }
    if(!m_canExport) {
        qWarning() << m_exportDisabledReason;
        return;
    }
    filename = cwGlobals::convertFromURL(filename);

    cwCave* cave = currentCave();
    if(cave != nullptr) {
        cwChipdataExportCaveTask* exportTask = new cwChipdataExportCaveTask();
        exportTask->setOutputFile(filename);
        exportTask->setData(cave->data());
        connect(exportTask, SIGNAL(finished()), SLOT(exporterFinished()));
        connect(exportTask, SIGNAL(stopped()), SLOT(exporterFinished()));
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
        if(!Trip.isNull()) {
            setCave(Trip->parentCave());
        }
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
        if(!Cave.isNull()) {
            Q_ASSERT(qobject_cast<cwCavingRegion*>(Cave->parent()) != nullptr);
            setCavingRegion(qobject_cast<cwCavingRegion*>(Cave->parent()));
        }
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
        rewireExternalCenterlineTracking();
        updateActions();
        emit cavingRegionChanged();
    }
}

/**
  \brief Tears down every signal connection that drives the canExport gate
  and re-wires against the current region, its caves, and each cave's trips.
  Called whenever the region pointer changes or any structural mutation
  (cave or trip insert/remove) fires. Always finishes with a recomputeCanExport()
  so the gate state stays consistent with the freshly-wired set.
  */
void cwSurveyExportManager::rewireExternalCenterlineTracking() {
    for (const auto& connection : m_externalCenterlineConnections) {
        QObject::disconnect(connection);
    }
    m_externalCenterlineConnections.clear();

    if (!CavingRegion.isNull()) {
        cwCavingRegion* region = CavingRegion.data();
        m_externalCenterlineConnections.append(connect(
            region, &cwCavingRegion::insertedCaves,
            this, &cwSurveyExportManager::rewireExternalCenterlineTracking));
        m_externalCenterlineConnections.append(connect(
            region, &cwCavingRegion::removedCaves,
            this, &cwSurveyExportManager::rewireExternalCenterlineTracking));

        const QList<cwCave*> caves = region->caves();
        for (cwCave* cave : caves) {
            m_externalCenterlineConnections.append(connect(
                cave, &cwCave::externalCenterlineChanged,
                this, &cwSurveyExportManager::recomputeCanExport));
            m_externalCenterlineConnections.append(connect(
                cave, &cwCave::insertedTrips,
                this, &cwSurveyExportManager::rewireExternalCenterlineTracking));
            m_externalCenterlineConnections.append(connect(
                cave, &cwCave::removedTrips,
                this, &cwSurveyExportManager::rewireExternalCenterlineTracking));

            const QList<cwTrip*> trips = cave->trips();
            for (cwTrip* trip : trips) {
                m_externalCenterlineConnections.append(connect(
                    trip, &cwTrip::externalCenterlineChanged,
                    this, &cwSurveyExportManager::recomputeCanExport));
            }
        }
    }

    recomputeCanExport();
}

/**
  \brief Walks every cave and trip in the current region looking for an
  attachment. Updates m_canExport / m_exportDisabledReason and emits
  canExportChanged() only when the gate flips. Scope-state detection
  (non-empty stationPrefix on cwTrip) lands in Phase 3; until then the
  externalCenterline check covers every entity that can break round-trip
  through Survex / Compass exports.
  */
void cwSurveyExportManager::recomputeCanExport() {
    const auto anyAttachment = [this]() {
        if (CavingRegion.isNull()) {
            return false;
        }
        const QList<cwCave*> caves = CavingRegion->caves();
        for (cwCave* cave : caves) {
            if (!cave->externalCenterline().isEmpty()) {
                return true;
            }
            const QList<cwTrip*> trips = cave->trips();
            for (cwTrip* trip : trips) {
                if (!trip->externalCenterline().isEmpty()) {
                    return true;
                }
            }
        }
        return false;
    };

    const bool newCanExport = !anyAttachment();
    if (newCanExport == m_canExport) {
        return;
    }
    m_canExport = newCanExport;
    m_exportDisabledReason = newCanExport ? QString() : kExportDisabledReason;
    emit canExportChanged();
}
