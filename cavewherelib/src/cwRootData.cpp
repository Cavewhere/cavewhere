/**************************************************************************
**
**    Copyright (C) 2013 by Philip Schuchardt
**    www.cavewhere.com
**
**************************************************************************/

//Our includes
#include "cwRootData.h"
#include "cwRegionTreeModel.h"
#include "cwCavingRegion.h"
#include "cwLinePlotManager.h"
#include "cwScrapManager.h"
#include "cwProject.h"
#include "cwTrip.h"
#include "cwTripCalibration.h"
#include "cwSurveyExportManager.h"
#include "cwSurveyImportManager.h"
#include "cwItemSelectionModel.h"
#include "cwQMLReload.h"
#include "cwLicenseAgreement.h"
#include "cwRegionSceneManager.h"
#include "cwTaskManagerModel.h"
#include "cwPageSelectionModel.h"
#include "cwSettings.h"
#include "cwImageCompressionUpdater.h"
#include "cwAddImageTask.h"
#include "cwJobSettings.h"
#include "cwSurveyNoteModel.h"

//Qt includes
#include <QItemSelectionModel>
#include <QUndoStack>
#include <QSettings>
#include <QStandardPaths>
#include <QApplication>
#include <QStyle>
#include <QProcess>
#include <QDesktopServices>
#include <QClipboard>

//Generated files from qbs
#include "cavewhereVersion.h"

cwRootData::cwRootData(QObject *parent) :
    QObject(parent),
    DefaultTrip(new cwTrip(this)),
    DefaultTripCalibration(new cwTripCalibration(this))
{
    cwSettings::initialize(); //Init's a singleton

    //Task Manager, allows the users to see running tasks
    TaskManagerModel = new cwTaskManagerModel(this);
    FutureManagerModel = new cwFutureManagerModel(this);

    //Create the project, this saves and load data
    Project = new cwProject(this);
    // Project->setTaskManager(TaskManagerModel);
    Project->setFutureManagerToken(FutureManagerModel);

    Region = Project->cavingRegion();
    Region->setUndoStack(undoStack());

    //Setup the region tree
    RegionTreeModel = new cwRegionTreeModel(Project);
    RegionTreeModel->setCavingRegion(Region);

    //Setup the loop closer
    LinePlotManager = new cwLinePlotManager(Project);
    LinePlotManager->setRegion(Region);

    //Setup the scrap manager
    ScrapManager = new cwScrapManager(Project);
    ScrapManager->setProject(Project);
    ScrapManager->setRegionTreeModel(RegionTreeModel);
    ScrapManager->setLinePlotManager(LinePlotManager);
    ScrapManager->setFutureManagerToken(FutureManagerModel->token());

    //Setup the survey import manager
    SurveyImportManager = new cwSurveyImportManager(Project);
    SurveyImportManager->setCavingRegion(Region);
    SurveyImportManager->setUndoStack(undoStack());
    SurveyImportManager->setErrorModel(Project->errorModel());

    QuickView = nullptr;

    //For debugging
    QMLReloader = new cwQMLReload(this);

    //For license agreement
    License = new cwLicenseAgreement(this);
    License->setVersion(version());

    RegionSceneManager = new cwRegionSceneManager(this);
    RegionSceneManager->setCavingRegion(Region);

    ScrapManager->setRenderScraps(RegionSceneManager->scraps());
    LinePlotManager->setRenderLinePlot(RegionSceneManager->linePlot());

    PageSelectionModel = new cwPageSelectionModel(this);

    cwImageCompressionUpdater* imageUpdater = new cwImageCompressionUpdater(this);
    imageUpdater->setFutureToken(FutureManagerModel->token());
    imageUpdater->setRegionTreeModel(RegionTreeModel);

    auto updateAutomaticUpdate = [this]()
    {
        bool autoUpdate = cwJobSettings::instance()->automaticUpdate();
        LinePlotManager->setAutomaticUpdate(autoUpdate);
        ScrapManager->setAutomaticUpdate(autoUpdate);
    };

    updateAutomaticUpdate();

    connect(cwJobSettings::instance(), &cwJobSettings::automaticUpdateChanged,
            this, updateAutomaticUpdate);

    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, [&]() { Project->waitSaveToFinish(); });
}

/**
Gets undoStack
*/
QUndoStack* cwRootData::undoStack() const {
    return Project->undoStack();
}

/**
Sets quickWindow
*/
void cwRootData::setQuickView(QQuickView* quickView) {
    if(QuickView != quickView) {
        QuickView = quickView;
        //        QMLReloader->setQuickView(QuickView);
        emit quickWindowChanged();
    }
}

/**
 * @brief cwRootData::version
 * @return
 *
 * Returns the current version of cavewhere
 */
QString cwRootData::version() const {
    return CavewhereVersion; //Automatically generated from qbs in cavewhereVersion.h
}

/**
* @brief cwRootData::setLeadsVisible
* @param leadsVisible
*
* This function is temperary, should be moved to a layer manager
*/
void cwRootData::setLeadsVisible(bool leadsVisible) {
    if(LeadsVisible != leadsVisible) {
        LeadsVisible = leadsVisible;
        emit leadsVisibleChanged();
    }
}

/**
*
*/
void cwRootData::setStationVisible(bool stationsVisible) {
    if(StationVisible != stationsVisible) {
        StationVisible = stationsVisible;
        emit stationsVisibleChanged();
    }
}

/**
* @brief cwRootData::lastDirectory
* @return Returns the last directory open by the file dialog. If the directory doesn't exist, this opens the desktopLocation
*/
QUrl cwRootData::lastDirectory() const {
    QSettings settings;
    QUrl dir = settings.value("LastDirectory", QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation))).toUrl();
    return dir;
}

/**
* @brief cwRootData::setLastDirectory
* @param lastDirectory - Set the last directory that was opened by the user. This is useful for
* propertly position the file dialogs
*/
void cwRootData::setLastDirectory(QUrl lastDirectory) {
    QFileInfo info(lastDirectory.toLocalFile());
    QUrl dir = QUrl::fromLocalFile(info.path());

    if(this->lastDirectory() != dir) {
        QSettings settings;
        settings.setValue("LastDirectory", dir);
        emit lastDirectoryChanged();
    }
}

cwSettings* cwRootData::settings() const {
    return cwSettings::instance();
}

void cwRootData::setPageView(cwPageView *value) {
    if (m_pageView != value) {
        m_pageView = value;
        emit pageViewChanged();
    }
}

QString cwRootData::cwRootData::supportImageFormats() const {
    return cwProject::supportedImageFormats();
}

QUrl cwRootData::cavewhereImageUrl(int id) const
{
    return cwImageProvider::imageUrl(id);
}

void cwRootData::newProject()
{
    PageSelectionModel->clearHistory();
    PageSelectionModel->gotoPageByName(nullptr, "View");
    Project->newProject();
}

int cwRootData::titleBarHeight() const
{
    Q_ASSERT(dynamic_cast<QApplication*>(QApplication::instance()) != nullptr);
    return static_cast<QApplication*>(QApplication::instance())->style()->pixelMetric(QStyle::PM_TitleBarHeight);
}

void cwRootData::showInFolder(const QString &path) const
{
    QFileInfo info(path);
#if defined(Q_OS_WIN)
    QStringList args;
    if (!info.isDir())
        args << "/select,";
    args << QDir::toNativeSeparators(path);
    if (QProcess::startDetached("explorer", args))
        return;
#elif defined(Q_OS_MAC)
    QStringList args;
    args << "-e";
    args << "tell application \"Finder\"";
    args << "-e";
    args << "activate";
    args << "-e";
    args << "select POSIX file \"" + path + "\"";
    args << "-e";
    args << "end tell";
    args << "-e";
    args << "return";
    if (!QProcess::execute("/usr/bin/osascript", args))
        return;
#endif
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.isDir()? path : info.path()));

}

void cwRootData::copyText(const QString &text) const
{
    auto clipboard = QGuiApplication::clipboard();
    clipboard->setText(text);

}
