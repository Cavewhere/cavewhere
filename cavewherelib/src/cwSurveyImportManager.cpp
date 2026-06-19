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
#include "cwCompassImporter.h"
#include "cwWallsImporter.h"
#include "cwCavingRegion.h"
#include "cwTrip.h"
#include "cwSurveyChunk.h"
#include "cwStation.h"
#include "cwShot.h"
#include "cwSurveyImportManager.h"
#include "cwErrorListModel.h"

//Qt includes
#include <QFileDialog>
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
        m_futureManagerToken.addJob({ QFuture<void>(future), QStringLiteral("Compass import") });
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
    QStringList filenames;
    foreach(QUrl url, urls) {
        filenames.append(url.toLocalFile());
    }
    return filenames;
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
