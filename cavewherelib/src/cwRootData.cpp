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
#include "cwGitHubIntegration.h"
#include "cwRemoteAccountModel.h"
#include "cwRemoteAccountSelectionModel.h"
// #include "cwImageCompressionUpdater.h"
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
    m_account(new QQuickGit::Account(this)),
    m_accountWatcher(new QQuickGit::AccountSettingWatcher(this)),
    m_repositoryModel(new cwRepositoryModel(this)),
    m_gitHubIntegration(nullptr),
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
    m_repositoryModel->setProject(Project);

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

    //Setup the note lidar manager
    NoteLiDARManager = new cwNoteLiDARManager(Project);
    NoteLiDARManager->setProject(Project);
    NoteLiDARManager->setLinePlotManager(LinePlotManager);
    NoteLiDARManager->setRegionTreeModel(RegionTreeModel);
    NoteLiDARManager->setFutureManagerToken(FutureManagerModel->token());

    //Setup the survey import manager
    SurveyImportManager = new cwSurveyImportManager(Project);
    SurveyImportManager->setCavingRegion(Region);
    SurveyImportManager->setUndoStack(undoStack());
    SurveyImportManager->setErrorModel(Project->errorModel());

    //Save account settings to QSetting
    m_accountWatcher->setPerson(m_account);

    QuickView = nullptr;

    //For debugging
    QMLReloader = new cwQMLReload(this);

    //For license agreement
    License = new cwLicenseAgreement(this);
    License->setVersion(version());

    RegionSceneManager = new cwRegionSceneManager(this);
    RegionSceneManager->setCavingRegion(Region);

    ScrapManager->setRenderScraps(RegionSceneManager->items()); //scraps());
    LinePlotManager->setRenderLinePlot(RegionSceneManager->linePlot());
    NoteLiDARManager->setRender(RegionSceneManager->items());

    PageSelectionModel = new cwPageSelectionModel(this);

    // cwImageCompressionUpdater* imageUpdater = new cwImageCompressionUpdater(this);
    // imageUpdater->setFutureToken(FutureManagerModel->token());
    // imageUpdater->setRegionTreeModel(RegionTreeModel);

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

QUrl cwRootData::cavewhereImageUrl(const QString& path) const
{
    return cwImageProvider::imageUrl(path);
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
#elif defined(Q_OS_MAC) && QT_CONFIG(process)
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

QString cwRootData::fileBaseName(const QString &path) const
{
    QFileInfo info(path);
    return info.baseName();
}

QString cwRootData::fileName(const QString &path) const
{
    QFileInfo info(path);
    return info.fileName();
}

bool cwRootData::mobileBuild() const
{
    return isMobileBuild();
}

//Don't inline this because it depends on private CW_MOBILE_BUILD define
bool cwRootData::isMobileBuild()
{
#ifdef CW_MOBILE_BUILD
    return true;
#else
    return false;
#endif
}



cwNoteLiDARManager *cwRootData::noteLiDARManager() const
{
    return NoteLiDARManager;
}

cwGitHubIntegration* cwRootData::gitHubIntegration() const
{
    if (!m_gitHubIntegration) {
        m_gitHubIntegration = new cwGitHubIntegration(const_cast<cwRootData*>(this));
    }
    return m_gitHubIntegration;
}

cwRemoteAccountModel* cwRootData::remoteAccountModel() const
{
    if (!m_remoteAccountModel) {
        m_remoteAccountModel = new cwRemoteAccountModel(const_cast<cwRootData*>(this));
    }
    return m_remoteAccountModel;
}

