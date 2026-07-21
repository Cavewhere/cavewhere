/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwSurveyImportManager.h"
#include "cwImportTreeDataDialog.h"
#include "cwSurvexImporter.h"
#include "cwSurvexGlobalData.h"
#include "cwTreeImportDataNode.h"
#include "cwCompassImporter.h"
#include "cwWallsImporter.h"
#include "cwCavingRegion.h"
#include "cwCave.h"
#include "cwTrip.h"
#include "cwTeam.h"
#include "cwTripCalibration.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyImportManager.h"
#include "cwErrorListModel.h"
#include "cwError.h"
#include "cwGlobals.h"

//Qt includes
#include <QFileDialog>
#include <QPointer>
#include <QSettings>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QWindow>

//Async includes
#include <asyncfuture.h>

namespace {
    //Transient parent keeps the dialog above the main window on macOS (issue #481).
    QWindow* mainQuickWindow() {
        const auto topLevel = QGuiApplication::topLevelWindows();
        for(QWindow* candidate : topLevel) {
            if(qobject_cast<QQuickWindow*>(candidate) && candidate->isVisible()) {
                return candidate;
            }
        }
        return nullptr;
    }

    void launchImportDialog(cwImportTreeDataDialog::Names names,
                            cwTreeDataImporter* importer,
                            cwCavingRegion* region,
                            QUndoStack* undoStack,
                            const QStringList& files) {
        auto* dialog = new cwImportTreeDataDialog(names, importer, region, mainQuickWindow());
        dialog->setUndoStack(undoStack);
        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->open();
        dialog->setInputFiles(files);
    }
}

cwSurveyImportManager::cwSurveyImportManager(QObject *parent) :
    QObject(parent),
    CavingRegion(nullptr),
    MessageListFont(QFontDatabase::systemFont(QFontDatabase::FixedFont))
{
}

cwSurveyImportManager::~cwSurveyImportManager()
{
    //Cancel any in-flight imports, then join their worker threads. The
    //.context(this, ...) continuations are auto-disconnected as this is
    //destroyed, so they won't run against a dead manager.
    for(auto& future : m_compassFutures) {
        future.cancel();
    }
    for(auto& future : m_compassFutures) {
        future.waitForFinished();
    }
}

void cwSurveyImportManager::setFutureManagerToken(cwFutureManagerToken token)
{
    m_futureManagerToken = token;
}

void cwSurveyImportManager::setCavingRegion(cwCavingRegion *region)
{
    if(CavingRegion != region) {
        CavingRegion = region;
        emit cavingRegionChanged();
    }
}

/**
  \brief Opens the survex importer dialog
  */
void cwSurveyImportManager::importSurvex() {

    QSettings settings;
    QString lastFile = settings.value(ImportSurvexKey).toString();
    QString filename = QFileDialog::getOpenFileName(nullptr, "Import Survex", lastFile, "Survex *.svx");

    if (QFileInfo(filename).exists()) {
        settings.setValue(ImportSurvexKey, filename);
        cwImportTreeDataDialog::Names names{"Survex Importer", "Survex Errors"};
        launchImportDialog(names, new cwSurvexImporter(), cavingRegion(), UndoStack, {filename});
    }
}

/**
 * @brief cwSurveyImportManager::importSurvexToTrip
 *
 * Imports a single .svx file directly into the supplied trip. The trip must
 * be empty (zero chunks) — this prevents destroying user-entered data. Trip
 * date, team and calibration are copied from the first imported sub-trip;
 * survey chunks from every imported sub-trip are flattened into the target.
 * Parser warnings and the not-empty rejection are pushed to ErrorModel.
 */
void cwSurveyImportManager::importSurvexToTrip(const QUrl& fileUrl, cwTrip* trip)
{
    if (!trip) return;

    if (trip->chunkCount() > 0) {
        if (ErrorModel) {
            ErrorModel->append(cwError(
                QStringLiteral("Cannot import survex into trip \"%1\": it already has survey chunks. Add a new empty trip first.")
                    .arg(trip->name()),
                cwError::Fatal));
        }
        return;
    }

    const QString path = cwGlobals::importPathFromUrl(fileUrl);
    if (path.isEmpty()) return;

    auto importer = new cwSurvexImporter(this);
    importer->setInputFiles(QStringList{path});

    QPointer<cwTrip> tripGuard(trip);
    connect(importer, &cwTask::finished, this, [this, importer, tripGuard]() {
        if (importer->hasParseErrors() && ErrorModel) {
            const QStringList errs = importer->parseErrors();
            for (const QString& msg : errs) {
                ErrorModel->append(cwError(msg, cwError::Warning));
            }
        }

        if (!tripGuard) {
            importer->deleteLater();
            return;
        }

        // Re-check chunk count: trip may have gained chunks while the
        // background parse was running.
        if (tripGuard->chunkCount() > 0) {
            if (ErrorModel) {
                ErrorModel->append(cwError(
                    QStringLiteral("Survex import dropped: trip \"%1\" gained chunks while parsing.")
                        .arg(tripGuard->name()),
                    cwError::Fatal));
            }
            importer->deleteLater();
            return;
        }

        // Mark every parsed top-level block as a Trip so cavesHelper()
        // walks them. The default ImportType is NoImport, which would
        // otherwise drop all chunks.
        const QList<cwTreeImportDataNode*> rootNodes = importer->data()->nodes();
        for (cwTreeImportDataNode* node : rootNodes) {
            node->setImportType(cwTreeImportDataNode::Trip);
        }

        const QList<cwCave*> caves = importer->data()->caves();
        bool metadataCopied = false;
        for (cwCave* cave : caves) {
            const QList<cwTrip*> trips = cave->trips();
            for (cwTrip* importedTrip : trips) {
                if (!metadataCopied) {
                    tripGuard->setDate(importedTrip->date());
                    if (importedTrip->team() && tripGuard->team()) {
                        tripGuard->team()->setData(importedTrip->team()->data());
                    }
                    if (importedTrip->calibrations() && tripGuard->calibrations()) {
                        tripGuard->calibrations()->setData(importedTrip->calibrations()->data());
                    }
                    metadataCopied = true;
                }
                const QList<cwSurveyChunk*> chunks = importedTrip->chunks();
                for (cwSurveyChunk* src : chunks) {
                    auto* copy = new cwSurveyChunk();
                    copy->setData(src->data());
                    tripGuard->addChunk(copy);
                }
            }
        }
        for (cwCave* cave : caves) cave->deleteLater();

        importer->deleteLater();
    });

    importer->start();
}

/**
 * @brief cwSurveyImportManager::importCompassDataFile
 *
 * Open a compass file to import
 */
void cwSurveyImportManager::importCompassDataFile(QList<QUrl> filenames)
{
    QStringList dataFiles = urlsToStringList(filenames);

    emit messagesCleared();

    //Each import runs independently so overlapping imports both complete (a
    //restart-style cancel would drop the caves of the in-flight import).
    auto future = cwCompassImporter::run(dataFiles);
    m_compassFutures.append(future);

    if(m_futureManagerToken.isValid()) {
        m_futureManagerToken.addJob(future, QStringLiteral("Compass import"));
    }

    //.context(this, ...) delivers the result on the main thread and is
    //auto-disconnected when this manager is destroyed.
    AsyncFuture::observe(future).context(this, [this, future]() {
        onCompassFinished(future.result());
        //QFuture has no operator==, so drop every finished import (this one
        //included) rather than removing by value.
        m_compassFutures.removeIf([](const QFuture<cwCompassImporter::Result>& f) {
            return f.isFinished();
        });
    });
}

void cwSurveyImportManager::waitForCompassToFinish()
{
    //Test-only: spin the event loop (via AsyncFuture::waitForFinished) so the
    //queued .context() continuation runs and the caves/messages are delivered
    //before returning. Iterate a copy because the continuation mutates
    //m_compassFutures.
    const auto futures = m_compassFutures;
    for(const auto& future : futures) {
        AsyncFuture::waitForFinished(future);
    }
}

/**
 * @brief cwSurveyImportManager::onCompassFinished
 * @param result - The caves and parse messages produced by the import
 *
 * Called on the main thread when a compass import has finished running
 */
void cwSurveyImportManager::onCompassFinished(const cwCompassImporter::Result& result)
{
    UndoStack->beginMacro("Compass Import");

    //Add new caves
    for(const auto& cave : result.caves) {
        cwCave* newCave = new cwCave();
        newCave->setData(cave);
        CavingRegion->addCave(newCave);
    }

    UndoStack->endMacro();

    //Report any parse warnings as a single batch
    if(ErrorModel && !result.messages.isEmpty()) {
        QList<cwError> errors;
        errors.reserve(result.messages.size());
        for(const auto& message : result.messages) {
            errors.append(cwError(message, cwError::Warning));
        }
        ErrorModel->append(errors);
    }
}

/**
  \brief Opens the Walls importer dialog
  */
void cwSurveyImportManager::importWalls() {

    QSettings settings;
    QString lastFile = settings.value(ImportWallsKey).toString();
    QString filename = QFileDialog::getOpenFileName(nullptr, "Import Walls", lastFile, "Walls *.wpj");

    if (QFileInfo(filename).exists()) {
        settings.setValue(ImportWallsKey, filename);
        cwImportTreeDataDialog::Names names{"Walls Importer", "Walls Errors"};
        launchImportDialog(names, new cwWallsImporter(), cavingRegion(), UndoStack, {filename});
    }
}

void cwSurveyImportManager::importWallsSrv() {
    QSettings settings;
    QString lastDir = settings.value(ImportWallsKey).toString();
    QStringList filenames = QFileDialog::getOpenFileNames(nullptr, "Import Walls", lastDir, "Walls *.srv");

    if (!filenames.isEmpty()) {
        settings.setValue(ImportWallsKey, filenames[0]);
        cwImportTreeDataDialog::Names names{"Walls Importer", "Walls Errors"};
        launchImportDialog(names, new cwWallsImporter(), cavingRegion(), UndoStack, filenames);
    }
}

/**
 * @brief cwSurveyImportManager::wallsMessages
 * @param message
 *
 * Reports messages
 * TODO: Make this report to the gui, in a meaning full way
 */
void cwSurveyImportManager::wallsMessages(QString severity, QString message, QString source,
                   int startLine, int startColumn, int endLine, int endColumn)
{
    qDebug() << "Walls Importer:" << message;
    emit messageAdded(severity, message, source, startLine, startColumn, endLine, endColumn);
}

/**
 * @brief cwSurveyImportManager::urlsToStringList
 * @param urls
 * @return The converted urls as a stringlist
 */
QStringList cwSurveyImportManager::urlsToStringList(QList<QUrl> urls)
{
    return cwGlobals::importPathsFromUrls(urls);
}

/**
 * @brief cwSurveyImportManager::cavingRegion
 * @return Returns the current caving region that the import will add to
 */
cwCavingRegion *cwSurveyImportManager::cavingRegion() const
{
    return CavingRegion;
}

/**
* @brief cwSurveyImportManager::undoStack
* @return
*/
QUndoStack* cwSurveyImportManager::undoStack() const {
    return UndoStack;
}

/**
* @brief cwSurveyImportManager::setUndoStack
* @param undoStack
*/
void cwSurveyImportManager::setUndoStack(QUndoStack* undoStack) {
    if(UndoStack != undoStack) {
        UndoStack = undoStack;
        emit undoStackChanged();
    }
}

/**
*
*/
void cwSurveyImportManager::setErrorModel(cwErrorListModel* errorModel) {
    if(ErrorModel != errorModel) {
        ErrorModel = errorModel;
        emit errorModelChanged();
    }
}

/**
*
*/
cwErrorListModel* cwSurveyImportManager::errorModel() const {
    return ErrorModel;
}
